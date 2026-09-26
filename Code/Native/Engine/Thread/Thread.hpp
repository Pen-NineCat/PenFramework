// File /Native/Engine/Thread/Thread.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../Coroutine/CoroutineTask.hpp"
#include "../DebugTools/DebugVerify.hpp"
#include "../Object/SignalObject.hpp"
#include "../Utils/OwnerThread.hpp"
#include "ThreadTypes.hpp"
#include <atomic>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace PenEngine
{

	/// @brief 线程任务的观察结果
	/// @note 任务自身的终态集合为 { TaskInvalid, Finish, StopSuccess, StopFailed, HasException }；
	///       { Waiting, Running } 为非终态，终态一经写入不再改变（不变量 I2）；
	///       `Detach` 不属于任务状态，见其注释
	enum class ThreadTaskResult : U8
	{
		TaskInvalid,    ///< 默认构造：没有可启动的对象
		Waiting,        ///< 有任务，尚未启动（延迟启动且未 StartTask）
		Running,        ///< 工作中
		Finish,         ///< 正常结束，且从未受理过停止请求
		StopSuccess,    ///< 受理过停止请求，且任务体具备被停止的通道（可接收 std::stop_token）
		StopFailed,     ///< 受理过停止请求，但任务体没有可被通知的通道（不接收 std::stop_token）
		HasException,   ///< 任务体抛出；异常由本类捕获保存，Wait/WaitAsync 不抛出，用 GetException() 取回
		Detach          ///< **不是任务终态**：仅由 `Wait()` / `WaitAsync()` 在"本对象已 Detach"时返回，
		                ///< 表示"已放弃等待权、无法再等"；任务可能仍在运行，
		                ///< 其真实状态仍可用 `TaskResult()` 轮询
	};

	/// @brief 线程操作在非法状态下被调用
	/// @note 与 STL 同形的两个操作（`Join` / `Detach`）沿用 STL 的抛异常风格：
	///       Debug 下先由 `DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION` 中断，Release 下抛出本异常；
	///       其余带 bool 返回值的操作（`StartTask` / `RequestStop` / `LockThreadCore`）继续用返回值表达失败
	class BadThreadOperation : public Exception
	{
	public:
		/// @param Operation 操作名（如 "Join" / "Detach"）
		/// @param Detail 失败原因
		BadThreadOperation(std::string_view Operation, std::string_view Detail)
			: Exception("BadThreadOperation", std::string(Operation) + "：" + std::string(Detail))
		{}
	};

	/// @brief 基于 std::jthread 的线程任务对象
	///
	/// 用法：
	///   `Thread Immediate{ task, Arg1 };`                     // 立即启动
	///   `Thread deferred{ Thread::DeferredStart, task };`     // 延迟启动
	///   `deferred.Waitable();`                                // true
	///   `deferred.StartTask();`                               // 开闸
	///   `deferred.Wait();`                                    // 阻塞取终态
	///
	/// 线程模型（本类只做"一个任务 + 一个 jthread"的薄封装，不含线程池、不含调度器）：
	/// - 构造它的线程是 **owner 线程**；任务体运行在 **worker 线程**；
	/// - 权威状态是一件原子量（State::Result），信号只是通知手段；
	/// - `WaitAsync()` 是状态轮询（`ICoroutineInstruction`），**不依赖信号**，因此
	///   即使线程在开始等待之前就已结束也不会漏醒、不会永久挂起。
	///
	/// 信号与发射线程（**阅读本段前请先接受：ObjectSignal 本身不是线程安全的**）：
	/// - `ThreadStart`  —— 由 **owner 线程** 在 `StartTask()` 内发出（开闸之前，因此不与 worker 并发）；
	///   立即启动的线程不存在该信号的可观测时机（构造完成前不可能有接收者），故不发出；
	/// - `ThreadFinish` —— 由 **worker 线程** 在任务体退出时发出；
	/// - `ThreadStop`   —— 由 **worker 线程** 在任务体退出时发出，仅在"曾受理过停止请求"时发出；
	///   参数为"停止是否成功"，即终态是否为 `StopSuccess`；
	/// - 停止请求**落空**（无任务 / 尚未启动且已被否决 / 已终结 / 重复请求）**不发出任何信号**，
	///   只由 `RequestStop()` 返回 false 表达。原因是：worker 可能正在 Emit，owner 再 Emit 同一个
	///   信号就是两个线程同时改同一张槽位表（`SignalSlotTable::m_emitDepth` 是普通整数）。
	///
	/// 线程安全契约（不变量，改动本类前必读，改完同步更新）：
	///
	///   T1 同一信号的 Emit 永远只有一个线程：owner 只发 `ThreadStart`，worker 只发
	///      `ThreadStop` / `ThreadFinish`；`ThreadStart` 的 Emit 早于开闸，故两者不重叠。
	///   T2 槽位表的**写**（Connect / Disconnect / ScopedConnection 的建立与析构）只允许发生在
	///      两个窗口：(a) 构造完成 → 调用 `StartTask()` 之前；(b) 观察到终态之后。
	///      窗口之外（`Running` 期间）任何连接改动都是数据竞争。由于信号是公开成员，
	///      本类无法拦截直接调用 `signal.Connect(...)` 的写法，此约束靠本契约与评审保证。
	///   T3 worker 退出路径的次序固定为：提交终态 → Emit(ThreadStop?) → Emit(ThreadFinish)
	///      → `Result.store(终态, release)`。owner 侧对 `Result` 的 acquire 读与之同步，
	///      因此"观察到终态"蕴含"本次信号已发完"。
	///   T4 信号槽位回调在 **worker 线程** 上执行：回调内禁止调用一切单线程设施
	///      （`CoroutineScheduler::Post*` / `DeferredDestroyQueue` / `NotificationBox` /
	///      其它对象的信号 Emit/Connect / `PObject::DestroyLater`）；需要回到 owner 线程时，
	///      请自行把结果投递到线程安全的队列，由 owner 线程消费。
	///   T5 回调抛出的异常会从 worker 逃出并导致 `std::terminate`，因此本类在 worker 侧统一
	///      try/catch 吞掉（Debug 下由 `DEBUG_VERIFY_REPORT` 中断，Release 下静默）。
	///   T6 `~Thread` 二选一：未 detach ⇒ `request_stop()` + join（worker 的全部 Emit 都在槽位表析构
	///      之前完成）；已 detach ⇒ 只做"关闭信号发射"的握手（见 T8），不请求停止也不 join。
	///      反过来说：任务体若不响应停止请求，未 detach 的析构会**永久阻塞**。
	///   T7 延迟启动的闸门是 **一次性事件**（`std::promise<void>` + `std::future<void>`），
	///      且必须同时响应"开闸"与"停止"：停止路径经 `std::stop_callback` 复用同一个
	///      `OpenGate()`（原子仲裁保证 `set_value` 恰好一次）。否则"延迟启动的线程被析构"
	///      会永久阻塞在 join 上（`future::wait()` 本身不可中断）。
	///      `OpenGate()` 的调用者只有四处：构造期的立即启动、`StartTask()`、
	///      `Waiting` 态受理停止、worker 自己的 `std::stop_callback`（`Detach()` **不在**其中）。
	///   T8 信号发射与对象析构的握手：worker 每次 Emit 都在 `State::SignalMutex` 内先查
	///      `SignalsAllowed`；`~Thread` 在释放本对象之前加同一把锁并把它置 false。
	///      因此 detach 之后销毁本对象是安全的：worker 此后只碰 `shared_ptr<State>`，
	///      绝不再解引用 `Thread*`（代价：detach 后析构 ⇒ 之后的 ThreadStop/ThreadFinish 不再发出）。
	///
	/// "未启动"的判定与后果（**未启动 = 闸门未决，即 `Waitable()` 为真**）：
	///
	///   - `Wait()`   → 立即返回 `Waiting`（观察类接口不阻塞、不改状态）
	///   - `Join()`   → 抛 `BadThreadOperation`：worker 正停在闸门上没人放行，join 会**永久阻塞**
	///   - `Detach()` → 抛 `BadThreadOperation`：不允许"静默开闸并丢弃一个从未启动的任务"，
	///     也不允许把它永久留在闸门上
	///   - 想放弃未启动的任务：`RequestStop()`（受理后闸门打开、任务体不执行，终态为
	///     `StopSuccess` / `StopFailed`），或直接销毁本对象（析构会请求停止并 join）
	///   - 边界：`Waiting` 态**已受理停止**之后闸门已开、任务体不会执行，此时不算"未启动"，
	///     `Join()` / `Detach()` 都正常放行（`Waitable()` 已为 false）
	///
	/// Detach 之后的接口行为（`Detach` 放弃的是"等待权/句柄"，不是任务本身）；
	/// 注意 `Detach()` 只可能发生在"已启动"或"已受理停止"之后，故其后 `StartTask()` 必然返回 false：
	///
	///   | 接口             | 行为                                                        |
	///   | ---------------- | ----------------------------------------------------------- |
	///   | `Detach()`       | `jthread::detach()`；`Joinable()` 变 false                   |
	///   | `TaskResult()`   | **仍返回真实状态**（State 由 worker 继续更新）；永不返回 `Detach` |
	///   | `Wait()`         | 判断 `IsDetached()` ⇒ **立即返回 `Detach`**，不阻塞、不 join   |
	///   | `WaitAsync()`    | 同上；已在途的等待也会被唤醒（指令把 detached 视为"不必再等"） |
	///   | `RequestStop()`  | **仍然可用**（stop_source 属于 jthread 对象，未随 detach 丢失） |
	///   | `LockThreadCore` | 返回 false（不可 join ⇒ 拿不到有效句柄）                     |
	///   | `GetID()`        | 随 STL 变为 `std::thread::id{}`；`ThreadID()` 仍返回缓存的 ID |
	///   | `~Thread()`      | 不再 `request_stop()`、不再 join（与 STL 的 `~thread` 一致）  |
	///
	/// 其它纪律：
	/// - **禁止在 worker 线程内析构自身、`Wait()` / `Join()` 自身**（自 join 死锁，Debug 下由宏拦下）；
	/// - `Thread` 继承 `SignalObject`（不可移动、不可拷贝），因此容器里请放指针；
	/// - 只能在 owner 线程析构本对象（析构会 join 并发信号，跨线程析构与 owner 侧的连接改动竞争）；
	/// - `Join()` / `Detach()` 在"不可 join"或"任务尚未启动"时抛出 `BadThreadOperation`
	///   （Debug 下先中断），与 STL 的抛异常风格一致；其余操作失败一律用返回值表达；
	/// - 本类不是 `PObject`，**不能自行提交协程**：`WaitAsync()` 产出的任务需由调用方
	///   `CoroutineScheduler::PostCoroutineTask` / `host->StartCoroutine` 提交后才会被驱动。
	class Thread final : public SignalObject
	{
	public:
		/// @brief 延迟启动 tag：`Thread deferred{ Thread::DeferredStart, task, Args... };`
		struct DeferredStartT
		{
			explicit DeferredStartT() = default;
		};

		/// @brief 延迟启动 tag 的唯一实例，配合 `Thread::DeferredStart` 使用
		inline static constexpr DeferredStartT DeferredStart{};

		/// @brief 延迟启动时，线程真正开始执行时发出（owner 线程，`StartTask()` 内）
		/// @note 这里写 `ObjectSignal<>` 而不是 `ObjectSignal<void>`：两者语义完全相同
		///       （`ObjectSignal<void>` 只是 `ObjectSignal<>` 的转发特化），但
		///       `SignalObject::Connect(ObjectSignal<Args...>&, F&&)` 的推导会把 `ObjectSignal<void>`
		///       判成 Args = { void }，进而要求 `std::invocable<F&, void>` 而**编译失败**。
		///       用空包声明，带接收者的写法（`Other.Connect(thread.ThreadStart, Slot)`）才能编译
		ObjectSignal<> ThreadStart{ this };

		/// @brief 任务体退出时发出（worker 线程）
		/// @note 等待任务结束请用 `WaitAsync()`（状态轮询，不存在漏醒与竞争）；
		///       `co_await ThreadFinish` 只适用于"延迟启动且尚未 StartTask"的窗口——
		///       信号等待器会在 owner 线程上订阅槽位（改槽位表），而 Running 期间 worker 正在 Emit，
		///       两者并发即为 T2 所禁止的数据竞争；对立即启动的对象更是必然错过本次事件
		ObjectSignal<> ThreadFinish{ this };

		/// @brief 任务体退出且"曾受理过停止请求"时发出（worker 线程），参数 = 停止是否成功
		ObjectSignal<bool> ThreadStop{ this };

		// ---- 连接本对象的信号时的写法约束 ----
		// @note `SignalObject::Connect(SignalObject*, ...)` 有一条既有硬约束：
		//       接收者不得是信号自己的 owner（`receiver != m_owner`，见 SignalObject.hpp 的
		//       `An object must not connect to its own signal`）。因此
		//       `thread.Connect(thread.ThreadFinish, Slot)` 这类"自己连自己"的写法在 Debug 下会中断，
		//       应当改用：
		//         `ScopedConnection Guard = thread.ThreadFinish.ConnectScoped(Slot);`  // 析构即断开
		//       或由**另一个** SignalObject 作为接收者连接：
		//         `Other.Connect(thread.ThreadFinish, Slot);`

		/// @brief 默认构造：没有可启动的对象（TaskInvalid）
		Thread() noexcept = default;

		/// @brief 立即启动：构造期创建 jthread 并让任务体直接开始执行
		/// @param task 任务体，必须能以 `(Args...)` 或 `(std::stop_token, Args...)` 调用
		/// @param args 任务实参，按 `std::decay_t` 存储（与 std::thread 一致）
		/// @note 立即启动不存在 `ThreadStart` 的可观测时机（构造完成前不可能有接收者）
		/// @exception std::system_error 线程创建失败（与 std::thread 一致，不吞掉）
		template <typename F, typename... Args>
		explicit Thread(F&& task, Args&&... args)
		{
			auto [invoker, stopCapable] = BindTask(std::forward<F>(task), std::forward<Args>(args)...);
			Construct(false, std::move(invoker), stopCapable);
		}

		/// @brief 延迟启动：线程在构造期创建并阻塞在闸门上，`StartTask()` 才放行
		/// @note 线程创建失败同样抛 std::system_error（异常来自构造期，不会变成静默的 TaskInvalid）
		template <typename F, typename... Args>
		Thread(DeferredStartT, F&& task, Args&&... args)
		{
			auto [invoker, stopCapable] = BindTask(std::forward<F>(task), std::forward<Args>(args)...);
			Construct(true, std::move(invoker), stopCapable);
		}

		/// @brief 析构（T6）：未 detach ⇒ 请求停止并 join；已 detach ⇒ 只做信号发射握手
		/// @note 未 detach 且任务体不响应停止请求时本函数会永久阻塞；禁止在 worker 线程内析构自身
		~Thread() noexcept override;

		Thread(const Thread&) = delete;
		Thread& operator=(const Thread&) = delete;
		Thread(Thread&&) = delete;
		Thread& operator=(Thread&&) = delete;

		// ---- 控制 ----

		/// @brief 开始线程执行（仅对"延迟启动且未被停止否决策"的对象有效）
		/// @return true = 本次调用放行了任务体；false = 已启动过 / 已被停止请求否决 / 无任务
		/// @note 本函数返回时闸门一定已打开（即使 `ThreadStart` 的槽位抛出，也由作用域守卫保证）
		/// @note 调用本函数即进入 T2 的"冻结窗口"：此后到观察到终态之前不得改动任何连接
		/// @note 只在 owner 线程调用：Debug 下报告并中断，Release 抛 `BadThreadOperation`
		bool StartTask();

		/// @brief 请求停止（协作式，进程内不存在强杀线程的手段）
		/// @return true = 受理（当时处于 Waiting 或 Running，且此前未受理过）/ false = 落空
		/// @note 落空不发信号（T1），只看返回值
		/// @note `Waiting` 态受理时连启动决策一并否决：任务体将不执行，直接落终态
		/// @note 受理不等于停止一定会生效：任务体无视 `std::stop_token` 时会跑到自然结束，
		///       此情形由绑定期确定的 `stopCapable` 判定为 `StopSuccess` / `StopFailed`
		/// @note **`Detach()` 之后仍然可用**：stop_source 属于 jthread 对象，未随 detach 丢失
		bool RequestStop();

		// ---- 观察 ----

		/// @brief 当前状态
		[[nodiscard]] ThreadTaskResult TaskResult() const noexcept
		{
			if (m_core == nullptr)
				return ThreadTaskResult::TaskInvalid;

			return m_core->State->Result.load(std::memory_order_acquire);
		}

		/// @brief 取回任务体抛出的异常（仅 HasException 时非空，改写为无异常出口）
		/// @note 先 acquire 读 Result 再读异常本体：异常只在终态发布之前写一次，因此无竞争
		[[nodiscard]] std::exception_ptr GetException() const noexcept
		{
			if (TaskResult() != ThreadTaskResult::HasException)
				return nullptr;

			return m_core->State->Exception;
		}

		/// @brief 是否有"可以启动的对象"（即处于 Waiting 且启动决策尚未被停止否决）
		[[nodiscard]] bool Waitable() const noexcept
		{
			if (m_core == nullptr)
				return false;

			const State& taskState = *m_core->State;
			std::lock_guard lock{ taskState.Mutex };

			return taskState.Result.load(std::memory_order_relaxed) == ThreadTaskResult::Waiting
				&& taskState.Decision == State::StartDecision::Pending;
		}

		/// @brief 与 stl 语义一致：存在线程且尚未 join / detach
		/// @note "已结束但未 join"仍返回 true；`Detach()` 之后返回 false
		[[nodiscard]] bool Joinable() const noexcept
		{
			return m_core != nullptr && m_core->Thread.joinable();
		}

		/// @brief 本对象是否已放弃该线程（`Detach()` 之后为 true）
		[[nodiscard]] bool IsDetached() const noexcept
		{
			return m_core != nullptr && m_core->State->Detached.load(std::memory_order_acquire);
		}

		/// @brief 阻塞等待并 join，返回终态
		/// @return 已 detach ⇒ `Detach`（立即，不阻塞）；未启动 ⇒ `Waiting`（不阻塞）；
		///         无任务 ⇒ `TaskInvalid`；其余为 join 后的终态
		/// @note 只 join、不改状态：等待结束后 `Joinable()` 变为 false
		/// @note 禁止在 worker 线程内调用（自 join 死锁，Debug 下由宏拦下，Release 下直接返回当前状态）
		/// @note 本函数不抛出：任务体的异常保存在对象内，请用 `GetException()` 取回
		ThreadTaskResult Wait();

		/// @brief 阻塞 join（不等价于 `Wait()`：不返回状态）
		/// @exception BadThreadOperation 不可 join（无任务 / 已 join / 已 detach / 还未启动）
		/// @note "还未启动"即闸门未决（`Waitable()` 为真）：worker 停在闸门上没人放行，join 会永久阻塞；
		///       想放弃它请用 `RequestStop()`（受理后闸门打开、任务体不执行），或直接销毁本对象
		void Join();

		/// @brief 与 stl 语义基本一致的分离：放弃句柄，线程独立运行
		/// @exception BadThreadOperation 不可分离（无任务 / 已 join / 已 detach / 还未启动）
		/// @note 置 `Detached` 标志：`Wait()` / `WaitAsync()` 据此立即返回 `ThreadTaskResult::Detach`；
		///       由于只可能发生在"已启动"或"已受理停止"之后，故其后 `StartTask()` 必然返回 false
		/// @note `TaskResult()` 仍反映**真实**任务状态（worker 会继续写 State），`RequestStop()` 也仍然可用
		/// @note 析构本对象是安全的（见 T8 握手）：destructor 之后 worker 只碰共享状态，不再解引用本对象，
		///       代价是 `ThreadStop` / `ThreadFinish` 不再发出
		/// @note 只在 owner 线程调用：Debug 下报告并中断，Release 抛 `BadThreadOperation`
		void Detach();

		/// @brief 协程化的等待：中断到任务离开 `Running` 为止
		/// @retval TaskInvalid（无任务）
		/// @retval Detach（已分离，立即返回）
		/// @retval Waiting（尚未启动，立即返回）
		/// @retval ThreadTaskResult 任务离开Running时的状态
		/// @note 被 CoroutineScheduler::Drain() 打断时不伪造结果，返回当时的真实状态
		/// @note 只观察 Running → 终态 这一步：任务从未进入 Running 的情形（尚未启动，
		///       或"启动前已受理停止"）都立即返回 Waiting；要连这一步也等，请用 Wait()
		/// @note 等待期间发生Detach()会唤醒本次等待并返回 Detach
		[[nodiscard]] CoroutineTask<ThreadTaskResult> WaitAsync()
		{
			return WaitAsyncOn(m_core);
		}

		// ---- 线程身份（供 T4 自检）----

		/// @brief 当前线程是否为本对象的 owner 线程（构造它的线程）
		[[nodiscard]] bool IsOwnerThread() const noexcept
		{
			return m_ownerThread.IsOwner();
		}

		/// @brief 当前线程是否为本对象的 worker 线程（即任务体所在的线程）
		[[nodiscard]] bool IsWorkerThread() const noexcept
		{
			return m_core != nullptr && m_core->Thread.get_id() == std::this_thread::get_id();
		}

		/// @brief 没有 worker（默认构造）时的原生线程 ID
		static constexpr NativeThreadID EmptyThreadID = 0;

		/// @brief 没有 worker（默认构造）时的原生句柄
		static constexpr NativeThreadHandle EmptyNativeHandle = NativeThreadHandle{};

		[[nodiscard]] std::thread::id GetID() const noexcept
		{
			return m_core != nullptr ? m_core->Thread.get_id() : std::thread::id{};
		}

		/// @brief 任务线程的原生 ID（操作系统域）
		/// @return EmptyThreadID（即 0）表示没有 worker
		[[nodiscard]] NativeThreadID ThreadID() const noexcept
		{
			return m_core != nullptr ? m_core->ThreadId : EmptyThreadID;
		}

		/// @brief worker 的原生句柄
		/// @return EmptyNativeHandle 表示没有 worker
		[[nodiscard]] NativeThreadHandle NativeHandle() const noexcept
		{
			return m_core != nullptr ? m_core->Thread.native_handle() : EmptyNativeHandle;
		}

		/// @brief 逻辑处理器总数（跨处理器组的实际数目，首次查询后缓存）
		[[nodiscard]] static U32 HardwareConcurrency() noexcept;

		/// @brief 当前线程的原生 ID
		[[nodiscard]] static NativeThreadID ThisThreadID() noexcept;

		/// @brief 当前线程的原生句柄
		[[nodiscard]] static NativeThreadHandle ThisThreadNativeHandle() noexcept;

		/// @brief 当前线程此刻所在的逻辑处理器编号
		[[nodiscard]] static CoreID ThisCoreIndex() noexcept;

		/// @brief 让出当前线程剩余的时间片
		static void ThisThreadYield() noexcept
		{
			std::this_thread::yield();
		}

		// ---- 线程亲和性 ----

		/// @brief 把本对象的 worker 绑到某个核心（可跨线程调用）
		/// @param policy 绑定策略
		/// @return 是否成功；无 worker、句柄已失效、机器没有所请求的核都会返回 false
		/// @note 延迟启动的对象可以在 `StartTask()` 之前就绑定
		bool LockThreadCore(LockThreadCorePolicy policy = LockThreadCorePolicy::OnCurrentCore) noexcept;

		/// @brief 把调用者自身绑到某个核心
		static bool LockThisThreadCore(LockThreadCorePolicy policy = LockThreadCorePolicy::OnCurrentCore) noexcept;

	private:
		/// @brief 任务体的类型擦除形式：统一接收 stop_token，由本类决定是否注入给用户任务
		using TaskInvoker = std::move_only_function<void(std::stop_token)>;

		/// @brief 任务内核：**不引用 Thread 对象**，因此 worker、协程帧、被放弃的等待者都可安全共享
		/// @note `Mutex` 同时守护三件事：启动决策、停止请求簿记、终态提交。
		///       把"停止请求的簿记"与"终态判定"放进同一把锁，是为了杜绝
		///       "RequestStop 返回 true 但终态却是 Finish"的竞争窗口。
		///       它与闸门无关：闸门是一次性事件，由下面的 promise/future 独立承载
		struct State
		{
			/// @brief 启动决策（受 Mutex 保护）
			enum class StartDecision : U8
			{
				Pending,   ///< 延迟启动，闸门尚未决定
				Run,       ///< 允许任务体执行
				Drop       ///< 任务体不得执行（启动前就受理了停止请求，或对象被 Detach）
			};

			mutable std::mutex Mutex;

			/// @brief 启动闸门（T7）：一次性事件，只有一个等待者（worker 本体），故用 future 而非 shared_future
			/// @note 声明次序不可换：`GateFuture` 取自 `Gate`
			std::promise<void> Gate;
			std::future<void> GateFuture{ Gate.get_future() };

			/// @brief 开闸仲裁：`set_value` 只允许发生一次（原子 exchange 定胜负）
			std::atomic<bool> GateOpened{ false };

			StartDecision Decision = StartDecision::Pending;

			/// @brief 停止请求已受理（受 Mutex 保护）
			bool StopRequested = false;

			/// @brief 终态已提交（受 Mutex 保护）：此后任何停止请求都是落空
			bool TerminalCommitted = false;

			/// @brief 任务体是否具备被停止的通道（绑定期即确定，此后只读）
			bool StopCapable = false;

			/// @brief 本对象是否已 Detach（放弃等待权）；worker 不读它，只由 owner 侧的观察接口读
			std::atomic<bool> Detached{ false };

			/// @brief 观察用状态；在信号 Emit 之后以 release 写入（T3）
			std::atomic<ThreadTaskResult> Result{ ThreadTaskResult::Waiting };

			bool SignalsAllowed = true;

			/// @brief 信号发射握手（T8）：worker 的每次 Emit 都在 `SignalMutex` 内先查 `SignalsAllowed`；
			///        `~Thread` 加同一把锁把它置 false，从而保证"析构之后 worker 不再解引用 Thread*"
			std::mutex SignalMutex;

			/// @brief 任务体抛出的异常；在终态发布之前写一次，此后只读
			std::exception_ptr Exception;
		};

		/// @brief 任务内核的持有者
		/// @note 成员次序不可调整：jthread 声明在最后 ⇒ 最先析构 ⇒ join 发生时 State 与 invoker 仍存活
		struct Core
		{
			std::shared_ptr<State> State;
			std::shared_ptr<TaskInvoker> Invoker;

			/// @brief worker 的原生线程 ID：构造期从句柄读一次（此后只读）
			/// @note 缓存的理由：`native_handle()` 在 join 之后不再有效，而线程 ID 作为标识应当一直可查
			NativeThreadID ThreadId = EmptyThreadID;

			std::jthread Thread;
		};

		/// @brief WaitAsync 的轮询指令；定义在 .cpp（只被 WaitAsyncOn 使用）
		class StateInstruction;

		/// @brief 把任务与实参绑定为"接收 stop_token"的类型擦除可调用体，并给出其可停止性
		/// @return { 任务体, 是否可停止 }
		/// @note 可调用性判定与 std::jthread 保持一致：优先 `task(stop_token, args...)`，否则 `task(args...)`
		template <typename F, typename... Args>
		[[nodiscard]] static std::pair<std::shared_ptr<TaskInvoker>, bool> BindTask(F&& task, Args&&... args)
		{
			using TaskType = std::decay_t<F>;
			using ArgTuple = std::tuple<std::decay_t<Args>...>;

			static_assert(std::is_invocable_v<TaskType, std::stop_token, std::decay_t<Args>...>
				|| std::is_invocable_v<TaskType, std::decay_t<Args>...>,
				"Thread 的任务体必须能以 (Args...) 或 (std::stop_token, Args...) 调用");

			constexpr bool stopCapable = std::is_invocable_v<TaskType, std::stop_token, std::decay_t<Args>...>;

			auto invoker = [task = TaskType(std::forward<F>(task)), arguments = ArgTuple(std::forward<Args>(args)...)]
				(std::stop_token token) mutable
			{
				std::apply(
					[&task, &token](auto&... unpacked)
					{
						if constexpr (stopCapable)
							std::invoke(task, token, std::move(unpacked)...);
						else
							std::invoke(task, std::move(unpacked)...);
					},
					arguments);
			};

			return { std::make_shared<TaskInvoker>(std::move(invoker)), stopCapable };
		}

		/// @brief 建立内核与线程（构造期唯一入口，定义在 .cpp）
		void Construct(bool deferred, std::shared_ptr<TaskInvoker> invoker, bool stopCapable);

		/// @brief 任务体外壳：守闸门 → 跑任务 → 收异常 → 提交终态 → 发信号（定义在 .cpp）
		/// @note 整个函数不得抛出：任务体的异常在此收口，信号回调的异常在此吞掉
		/// @param Owner 本对象；worker 只用它发信号（T6+T8 保证：未 detach 时 join 已完成，
		///        已 detach 时析构会先握手关闭发射，故两种情况下都不会解引用已析构的对象）
		static void RunBody(Thread* Owner, const std::shared_ptr<State>& taskState, TaskInvoker& task, std::stop_token token) noexcept;

		/// @brief 打开启动闸门（定义在 .cpp）：`set_value` 只可能发生一次，多路径调用安全
		/// @note 三条路径共用它：`StartTask()` 开闸放行、`Waiting` 态受理停止时开闸置 Drop、
		///       `Detach()` 时开闸置 Drop；worker 侧经 `std::stop_callback` 也走这里（T7）
		static void OpenGate(State& taskState) noexcept;

		/// @brief `WaitAsync()` 的协程实现：只持内核，不触碰 Thread 对象，因此对象先析构也不悬垂
		static CoroutineTask<ThreadTaskResult> WaitAsyncOn(std::shared_ptr<Core> taskCore);

		/// @brief 任务内核；为空表示默认构造（TaskInvalid）
		std::shared_ptr<Core> m_core;

		/// @brief owner 线程（构造本对象的线程）
		OwnerThread m_ownerThread;
	};
}
