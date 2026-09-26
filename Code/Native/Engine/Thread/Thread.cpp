// File /Native/Engine/Thread/Thread.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Thread.hpp"
#include "../Coroutine/ICoroutineInstruction.hpp"
#include "../Utils/ResourceScopeGuard.hpp"
#include "Internal/ThreadPlatform.h"
#include <utility>

namespace PenEngine
{
	namespace
	{
		/// @brief 终态判定（唯一写入点，读到的输入都已在锁内串行化）
		/// @param exception 任务体抛出的异常（可为空）
		/// @param stopRequested 是否曾受理过停止请求（含 stop_token 已被析构路径请求的情况）
		/// @param stopCapable 任务体是否具备被停止的通道（能否接收 std::stop_token）
		/// @note 异常的优先级最高：否则"抛出异常的任务"会被记成正常结束，异常再也取不回来
		[[nodiscard]] ThreadTaskResult DetermineTerminal(const std::exception_ptr& exception, bool stopRequested, bool stopCapable) noexcept
		{
			if (exception != nullptr)
				return ThreadTaskResult::HasException;

			if (!stopRequested)
				return ThreadTaskResult::Finish;

			// 没有 stop_token 通道的任务无法被通知停止：请求注定失败，线程结束是它自己跑完的
			return stopCapable ? ThreadTaskResult::StopSuccess : ThreadTaskResult::StopFailed;
		}
	}

	/// @brief WaitAsync 的等待指令
	/// @note 它是轮询指令而非信号等待器：调度器每帧问一次 IsReady()，因此
	///       "线程在开始等待之前就结束"也不会漏醒（信号只能等下一次 Emit，做不到这一点）
	class Thread::StateInstruction final : public ICoroutineInstruction
	{
	public:
		explicit StateInstruction(std::shared_ptr<State> taskState) noexcept : m_state(std::move(taskState)) {}

		/// @brief 不变量 I4：本函数必须严格等于 `!ShouldWait()`，
		///        否则会退化成"永久挂起"（IsReady 恒 false）或"一步内忙循环"（恒 true 而 ShouldWait 仍为真）
		[[nodiscard]] bool IsReady() override
		{
			return !ShouldWait();
		}

		[[nodiscard]] bool ShouldWait() const noexcept
		{
			// 已分离（本对象放弃等待权）也视为"不必再等"：在途的等待会被唤醒并返回 Detach
			if (m_abandoned || m_state->Detached.load(std::memory_order_acquire))
				return false;

			return m_state->Result.load(std::memory_order_acquire) == ThreadTaskResult::Running;
		}

		[[nodiscard]] ThreadTaskResult CurrentResult() const noexcept
		{
			// Detach 的判定必须早于读 Result：它表示"本对象已无法等待"，不是任务状态
			if (m_state->Detached.load(std::memory_order_acquire))
				return ThreadTaskResult::Detach;

			return m_state->Result.load(std::memory_order_acquire);
		}

		/// @brief 供 CoroutineScheduler::Drain() 打断等待
		/// @note 不伪造结果：CurrentResult() 仍返回真实状态（通常仍是 Running），调用方据此决定是否重等
		bool Cancel() override
		{
			m_abandoned = true;
			return true;
		}
	private:
		std::shared_ptr<State> m_state;

		/// @brief 是否已被调度器放弃；只在调度器线程（owner 线程）读写，无需原子
		bool m_abandoned = false;
	};

	void Thread::OpenGate(State& taskState) noexcept
	{
		// 原子 exchange 定胜负：无论几条路径同时开闸，set_value 都只可能被调用一次
		if (taskState.GateOpened.exchange(true, std::memory_order_acq_rel))
			return;

		try
		{
			taskState.Gate.set_value();
		}
		catch (...)
		{
			// 逻辑上不可达（仲裁已保证只调一次）；真到了这里也不能从 stop_callback 或析构里抛出
			DEBUG_VERIFY_REPORT(false, "Thread::OpenGate: set_value 意外失败（闸门仲裁被破坏）");
		}
	}

	void Thread::Construct(bool deferred, std::shared_ptr<TaskInvoker> invoker, bool stopCapable)
	{
		auto taskState = std::make_shared<State>();

		taskState->StopCapable = stopCapable;
		taskState->Decision = deferred ? State::StartDecision::Pending : State::StartDecision::Run;
		taskState->Result.store(
			deferred ? ThreadTaskResult::Waiting : ThreadTaskResult::Running, std::memory_order_relaxed);

		// 立即启动：闸门在构造期就是开的（future 会缓存这个值，worker 起来后 wait() 立刻返回）；
		// 延迟启动才需要等 StartTask / Detach / 停止来开闸
		if (!deferred)
			OpenGate(*taskState);

		auto taskCore = std::make_shared<Core>();
		taskCore->State = taskState;
		taskCore->Invoker = std::move(invoker);

		// 闭包只捕获 State、任务体与 owner：Core 持有 jthread，而 jthread 持有本闭包，
		// 捕获 Core 就成环了（环内两侧互相持有 ⇒ Core 永不析构 ⇒ join 永不发生）
		// owner 只用于发信号，其有效性由 T6 + T8 两重保证：
		//   未 detach ⇒ 析构会 join（worker 先结束）；已 detach ⇒ 析构先握手关闭信号发射
		std::shared_ptr<TaskInvoker> task = taskCore->Invoker;
		taskCore->Thread = std::jthread{ [this, taskState, task](std::stop_token token) noexcept
		{
			RunBody(this, taskState, *task, token);
		} };

		// 线程 ID 在此刻就能确定（不必等 worker 跑起来），缓存后 join 也依然可查
		taskCore->ThreadId = Internal::ThreadIDOf(taskCore->Thread.native_handle());

		m_core = std::move(taskCore);
	}

	void Thread::RunBody(Thread* owner, const std::shared_ptr<State>& taskState, TaskInvoker& task, std::stop_token token) noexcept
	{
		// ---- 闸门（T7）：一次性事件，等 StartTask / Detach / 停止 的开闸决策 ----
		// stop_token 被请求时（受理的停止、Detach 未开闸对象、析构的 request_stop）同样必须醒来，
		// 否则"延迟启动的对象被析构"会在 join 上永久阻塞：future::wait() 本身不可中断，
		// 故用 stop_callback 复用同一个 OpenGate；token 若已停止，该回调在构造时就会立即执行
		{
			std::stop_callback WakeOnStop{ token, [&taskState]() noexcept { OpenGate(*taskState); } };
			taskState->GateFuture.wait();
		}

		bool shouldRun = false;
		{
			std::lock_guard lock{ taskState->Mutex };
			shouldRun = taskState->Decision == State::StartDecision::Run;
		}

		// ---- 任务体：异常必须在此收口，逃出 worker 即为 std::terminate ----
		std::exception_ptr exception;
		if (shouldRun)
		{
			try
			{
				task(token);
			}
			catch (...)
			{
				exception = std::current_exception();
			}
		}

		// ---- 终态提交（T3 的第一步）：与 RequestStop 的簿记在同一把锁内串行化，
		//      从而杜绝"RequestStop 返回 true 但终态是 Finish"的竞争窗口 ----
		bool stopRequested = false;
		{
			std::lock_guard lock{ taskState->Mutex };
			taskState->TerminalCommitted = true;
			stopRequested = taskState->StopRequested || token.stop_requested();
		}

		// 异常本体先写入：它在终态发布之前只写这一次，之后只读（GetException 依赖这条次序）
		taskState->Exception = exception;

		const ThreadTaskResult terminal = DetermineTerminal(exception, stopRequested, taskState->StopCapable);

		// ---- 信号（T1：worker 是 ThreadStop / ThreadFinish 的唯一发射线程；T8：与析构握手）----
		// 回调抛异常会从 worker 逃出并导致 std::terminate，因此一律吞掉（T5）
		const auto emitSafely = [](auto&& Emit) noexcept
		{
			try
			{
				Emit();
			}
			catch (...)
			{
				DEBUG_VERIFY_REPORT(false, "Thread: 信号回调抛出异常，已在 worker 线程内吞掉（该回调之后的槽位本次不再收到）");
			}
		};

		{
			// T8：本对象可能已被 Detach 并析构 —— 加锁期间它无法完成析构，加锁后我们先看到
			// SignalsAllowed，析构则保证在看它为 true 时不会释放对象；因此这里的每一次 Emit
			// 都必然发生在"Owner 仍然有效"的窗口内
			std::lock_guard signalLock{ taskState->SignalMutex };

			if (taskState->SignalsAllowed)
			{
				if (stopRequested)
					emitSafely([owner, terminal]() { owner->ThreadStop.Emit(terminal == ThreadTaskResult::StopSuccess); });

				emitSafely([owner]() { owner->ThreadFinish.Emit(); });
			}
		}

		// ---- 发布（T3 的最后一步）：release 写终态，使"观察到终态"蕴含"上面的 Emit 与异常写入已完成" ----
		taskState->Result.store(terminal, std::memory_order_release);
	}

	bool Thread::StartTask()
	{
		if (m_core == nullptr)
		{
			DEBUG_VERIFY_REPORT(false, "Thread::StartTask: 没有可启动的任务（默认构造或任务缺失）");
			return false;
		}

		// ThreadStart 由 owner 线程发出（T1）：非 owner 调用会让"谁在 Emit"变得不可判定
		// @note Thread 不是核心插件，故异常用本类型自己的 BadThreadOperation（强度同样是
		//       Debug 报告并中断 / Release 抛出）
		PENFRAMEWORK_VERIFY_OWNER_THREAD_OR_THROW(m_ownerThread,
			"Thread::StartTask: 只允许在 owner 线程（构造本对象的线程）调用",
			BadThreadOperation, "StartTask", "只允许在 owner 线程（构造本对象的线程）调用");

		const std::shared_ptr<State>& taskState = m_core->State;
		{
			std::lock_guard lock{ taskState->Mutex };

			// 已启动过，或启动已被停止请求否决（RequestStop 在 Waiting 态受理时会置 Drop）
			if (taskState->Decision != State::StartDecision::Pending)
				return false;

			taskState->Decision = State::StartDecision::Run;
			taskState->Result.store(ThreadTaskResult::Running, std::memory_order_relaxed);
		}

		// 闸门必须无条件打开：ThreadStart 的槽位抛出异常也不能把 worker 永久留在闸门上
		ResourceScopeGuard GateOpener{ [&taskState]() noexcept { OpenGate(*taskState); } };

		// T1 / T2：本 Emit 发生在开闸之前，此刻 worker 还等在闸门上，因此不存在并发 Emit
		ThreadStart.Emit();

		return true;
	}

	bool Thread::RequestStop()
	{
		if (m_core == nullptr)
		{
			DEBUG_VERIFY_REPORT(false, "Thread::RequestStop: 没有任务，停止请求落空");
			return false;
		}

		const std::shared_ptr<State>& taskState = m_core->State;
		bool openGateForDrop = false;
		{
			std::lock_guard lock{ taskState->Mutex };

			// 落空：终态已提交 / 重复请求；落空一律不发信号（T1：worker 可能正在 Emit）
			if (taskState->TerminalCommitted || taskState->StopRequested)
				return false;

			taskState->StopRequested = true;

			// Waiting 态受理：连启动决策一并否决，任务体将不执行（开闸后直接落终态）
			if (taskState->Decision == State::StartDecision::Pending)
			{
				taskState->Decision = State::StartDecision::Drop;
				openGateForDrop = true;
			}
		}

		// 通知 stop_token：运行中的任务体可自行观察；停在闸门上的 worker 靠它脱离等待（T7 的 stop_callback）
		m_core->Thread.request_stop();

		// 兜底：即使 stop_callback 因故没被触发（例如 token 已被请求过），也要把闸门打开
		if (openGateForDrop)
			OpenGate(*taskState);

		return true;
	}

	ThreadTaskResult Thread::Wait()
	{
		if (m_core == nullptr)
			return ThreadTaskResult::TaskInvalid;

		// 已分离：本对象放弃了等待权（无权 join），立即返回 Detach 而不阻塞
		if (IsDetached())
			return ThreadTaskResult::Detach;

		// 可启动且尚未启动：阻塞等待没有意义，立即返回 Waiting（不阻塞）
		// @note 停止已在 Waiting 态被受理的对象不满足 Waitable()，会走到 join：
		//       它的任务体不会执行，join 很快返回 StopSuccess / StopFailed
		if (Waitable())
			return ThreadTaskResult::Waiting;

		if (IsWorkerThread())
		{
			DEBUG_VERIFY_REPORT(false, "Thread::Wait: 不允许在自身 worker 线程内等待自己（自 join 会死锁）");
			return TaskResult();
		}

		// 只 join，不请求停止：等待"任务自己结束"；要停就先 RequestStop()
		if (m_core->Thread.joinable())
			m_core->Thread.join();

		return TaskResult();
	}

	void Thread::Join()
	{
		// 与 STL 的 join 一致：不可 join 即抛异常（Debug 下先由宏中断）
		DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(m_core != nullptr && m_core->Thread.joinable(),
			"Thread::Join: 不可 join（无任务 / 已 join / 已 detach）",
			BadThreadOperation, "Join", "线程不可 join（无任务、已 join 或已 detach）");

		// 未启动（闸门未决）：worker 正停在闸门上，没人会放行它 ⇒ join 会**永久阻塞**
		// @note 边界：Waiting 态受理停止之后闸门已开、任务体不会执行，那种情形不算"未启动"，
		//       线程正在退出路上，join 很快返回（故判据就是 Waitable()）
		DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(!Waitable(),
			"Thread::Join: 任务尚未启动（闸门未开，join 会永久阻塞）",
			BadThreadOperation, "Join", "任务尚未启动：请先 StartTask()；若不想启动，请 RequestStop() 或销毁本对象");

		if (IsWorkerThread())
		{
			// 自 join 必然死锁：Debug 下由宏中断，Release 下也按"非法操作"抛出
			DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(false,
				"Thread::Join: 不允许在自身 worker 线程内 join 自己（会死锁）",
				BadThreadOperation, "Join", "不允许在自身 worker 线程内 join 自己");
		}

		m_core->Thread.join();
	}

	void Thread::Detach()
	{
		// 与 STL 的 detach 一致：不可分离即抛异常（Debug 下先由宏中断）
		DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(m_core != nullptr && m_core->Thread.joinable(),
			"Thread::Detach: 不可分离（无任务 / 已 join / 已 detach）",
			BadThreadOperation, "Detach", "线程不可分离（无任务、已 join 或已 detach）");

		// 未启动（闸门未决）：不允许分离一个从未启动的任务 —— 否则要么静默把它"开闸+丢弃"，
		// 要么把它永久留在闸门上。两种都不该是 Detach 的隐式行为，故明确报错
		// @note 边界同 Join：Waiting 态受理停止之后（闸门已开、任务体不会执行）可以正常分离
		DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(!Waitable(),
			"Thread::Detach: 任务尚未启动（不能分离一个从未启动的任务）",
			BadThreadOperation, "Detach", "任务尚未启动：请先 StartTask()；若不想启动，请 RequestStop() 或销毁本对象");

		PENFRAMEWORK_VERIFY_OWNER_THREAD_OR_THROW(m_ownerThread,
			"Thread::Detach: 只允许在 owner 线程（构造本对象的线程）调用",
			BadThreadOperation, "Detach", "只允许在 owner 线程（构造本对象的线程）调用");

		const std::shared_ptr<State>& taskState = m_core->State;

		// 放开等待权：此后 Joinable() 为 false、Wait() 立即返回 Detach、
		// TaskResult() 仍反映真实状态、RequestStop() 仍可用
		taskState->Detached.store(true, std::memory_order_release);

		// 真正放手：worker 继续独立运行（其共享状态由 jthread 内部持有，不随本对象析构而消失）
		m_core->Thread.detach();
	}

	CoroutineTask<ThreadTaskResult> Thread::WaitAsyncOn(std::shared_ptr<Core> taskCore)
	{
		if (taskCore == nullptr)
			co_return ThreadTaskResult::TaskInvalid;

		StateInstruction instruction{ taskCore->State };

		// 先查再等：Waiting / 终态在这里即刻返回，只有 Running 才进入轮询
		while (instruction.ShouldWait())
			co_await instruction;

		co_return instruction.CurrentResult();
	}

	// ================================================================
	// 线程工具与亲和性：全部转发到平台层（Internal/ThreadPlatform），本类不含平台代码
	// ================================================================

	U32 Thread::HardwareConcurrency() noexcept
	{
		return Internal::HardwareConcurrency();
	}

	NativeThreadID Thread::ThisThreadID() noexcept
	{
		return Internal::ThisThreadID();
	}

	NativeThreadHandle Thread::ThisThreadNativeHandle() noexcept
	{
		return Internal::ThisThreadNativeHandle();
	}

	CoreID Thread::ThisCoreIndex() noexcept
	{
		return Internal::ThisCoreIndex();
	}

	bool Thread::LockThreadCore(LockThreadCorePolicy policy) noexcept
	{
		// 无 worker / 已 join 时不做任何事：返回值就是错误通道，故不在 Debug 下中断
		if (m_core == nullptr || !m_core->Thread.joinable())
			return false;

		return Internal::LockThreadCore(m_core->Thread.native_handle(), policy);
	}

	bool Thread::LockThisThreadCore(LockThreadCorePolicy policy) noexcept
	{
		// 句柄传空 ⇒ 平台层作用于调用者自身
		return Internal::LockThreadCore(NativeThreadHandle{}, policy);
	}

	Thread::~Thread() noexcept
	{
		if (m_core == nullptr)
			return;

		if (IsWorkerThread())
		{
			// 在 worker 线程内销毁自己：join 自己必然死锁。此处已是契约违例（方法头已注明），
			// Detach 只为避免当场死锁；此后信号 Emit 会访问已析构的本对象 —— Debug 下由宏直接中断
			// @note 这条路径会跳过下面的 T8 握手（此刻本线程正持有 SignalMutex，加锁即自锁）
			DEBUG_VERIFY_REPORT(false, "Thread 不得在自身 worker 线程内析构（detach 后信号 Emit 会悬垂）");

			if (m_core->Thread.joinable())
				m_core->Thread.detach();

			return;
		}

		// T8 握手：先关闭信号发射。加锁期间 worker 无法完成 Emit，加锁后它能看到的
		// SignalsAllowed 必为 false；因此本函数返回后 worker 绝不会再解引用本对象。
		// 这是"Detach 之后销毁本对象仍然安全"的依据（代价：之后的 ThreadStop/ThreadFinish 不再发出）
		{
			std::lock_guard signalLock{ m_core->State->SignalMutex };
			m_core->State->SignalsAllowed = false;
		}

		// 已 detach 的线程不归本对象管：不请求停止、不 join（与 STL 的 ~thread 对非 joinable 的处理一致）
		if (!m_core->Thread.joinable())
			return;

		// T6：未 detach ⇒ 必然 join ⇒ worker 的全部 Emit 都在信号成员析构之前完成
		// @note 必须显式 request_stop()：jthread::join() 本身不请求停止，而 ~jthread 才会 ——
		//       若只 join，延迟启动且停在闸门上的对象会永久阻塞在这里（T7 的另一半）
		m_core->Thread.request_stop();
		m_core->Thread.join();
	}
}
