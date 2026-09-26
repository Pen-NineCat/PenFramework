// File /Native/Engine/Thread/ThreadPool.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../Exception/Exception.hpp"
#include "../Utils/OwnerThread.hpp"
#include "Thread.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stop_token>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace PenEngine
{
	/// @brief 队列满策略（队列容量由 `ThreadPoolConfig::MaxQueueSize` 给出）
	/// @note 无论哪种策略，策略动作都在**锁外**执行（绝不持队列锁跑任务或建线程）
	enum class ThreadPoolQueueFullPolicy : U8
	{
		Block,            ///< 阻塞提交方直到有空位。**在本池 worker 内提交时自动退化为 CallerRuns**（否则与本池互锁）
		Reject,           ///< 拒绝本次提交，返回 `RejectedQueueFull`
		TemporaryThread,  ///< 为**本次提交的任务**起一个一次性临时线程执行（不占 `MaxThreads` 名额，用完即弃）
		CallerRuns,       ///< 由提交线程亲自执行本次任务（线程创建失败时 `TemporaryThread` 也会退化到这里）
	};

	/// @brief 提交结果码
	enum class ThreadPoolCommitResultCode : U8
	{
		Accepted,                    ///< 已入队，`future` 有效
		ExecutedInline,              ///< 已由提交线程就地执行（CallerRuns / worker 内 Block 退化），`future` 有效
		ExecutedOnTemporaryThread,   ///< 已交给一次性临时线程（`future` 会在该线程完成后就绪）
		RejectedQueueFull,           ///< 队列满，被策略拒绝；**`future` 无效**
		RejectedStopping,            ///< 池正在关停或已关停；**`future` 无效**
	};

	/// @brief 提交结果
	/// @tparam R 任务返回值类型（`void` 任务即 `std::future<void>`）
	/// @note ⚠️ `code` 为 `RejectedQueueFull` / `RejectedStopping` 时 `future` **无效**，
	///       对它调用 `get()`/`wait()` 是未定义行为 —— 必须先判 `code`
	template <typename R>
	struct ThreadPoolCommitResult
	{
		ThreadPoolCommitResultCode Code = ThreadPoolCommitResultCode::RejectedStopping;
		std::future<R> Future;
	};

	/// @brief 线程池配置
	/// @note 构造期会做参数收敛（Debug 报告，Release 静默修正）：
	///       `MaxThreads == 0 ⇒ HardwareConcurrency()`、`CoreThreads` 夹到 `[1, MaxThreads]`、
	///       `MaxQueueSize == 0 && FullPolicy == Block` ⇒ 策略收敛为 `Reject`（容量 0 时 Block 永不满足）
	struct ThreadPoolConfig
	{
		/// 常驻线程量：构造期立即创建，此后不少于这个数量；**0 是合法配置**（不预建、按需扩容、空闲可降到 0）
		U32 CoreThreads = (Thread::HardwareConcurrency() / 2) == 0 ? 1 : (Thread::HardwareConcurrency() / 2);

		/// 最大线程量（不含"一次性临时线程"）；0 视为 `Thread::HardwareConcurrency()`
		U32 MaxThreads = Thread::HardwareConcurrency();

		/// 最大排队量；0 表示**不允许排队**（每次提交都直接命中队列满策略）
		Usize MaxQueueSize = 20;

		/// 队列满策略
		ThreadPoolQueueFullPolicy FullPolicy = ThreadPoolQueueFullPolicy::Reject;

		/// 超出常驻量部分的空闲存活期（仅在 AutomaticallyReleaseTimeoutThread 为 true 时生效）
		std::chrono::milliseconds KeepAlive{ 5000 };

		/// @brief 是否允许"空闲超时自动释放线程"
		/// @note true：空闲超过 `KeepAlive` 且超出常驻量的 worker 会**自行退出**（收缩到常驻量）；
		///       false（默认）：超时不会销毁任何线程，只能靠 `ShrinkToCore()` 显式收缩
		bool AutomaticallyReleaseTimeoutThread = false;
	};

	/// @brief 统计快照（`Stats()` 无副作用，不触发回收）
	struct ThreadPoolStats
	{
		U32 ActiveWorkers = 0;              ///< 存活 worker 数（原子精确：owner 创建时 +1，worker 退出时 -1）
		U32 BusyWorkers = 0;                ///< 正在执行任务的 worker 数（`ShrinkToCore` 的判据之一）
		U32 IdleWorkers = 0;                ///< 正停在队列上等待的 worker 数
		U32 WorkerSlots = 0;                ///< 池的 worker 槽位数（含尚未回收的已退出者）
		Usize QueuedTasks = 0;              ///< 队列中待执行任务数
		U64 Submitted = 0;                  ///< 累计提交（含最终被拒绝/就地执行/临时线程执行的）
		U64 Completed = 0;                  ///< 累计执行完成（含失败：异常另行计数）
		U64 Rejected = 0;                   ///< 累计被策略或关停拒绝
		U64 Discarded = 0;                  ///< 累计被丢弃（`Shutdown(false)` 时未开始的任务）
		U64 TaskExceptions = 0;             ///< 累计任务抛出的异常（已收口进 future，并计数）
		U64 TemporaryThreadsSpawned = 0;    ///< 累计一次性临时线程数
		U64 WorkerCrashes = 0;              ///< 累计"worker 意外死亡"（任务体逃出循环，正常情况下恒为 0）
	};

	/// @brief 基于 `Thread` 的线程池（常驻 + 按需扩容 + 空闲收缩）
	///
	/// 任务模型：
	/// - 任务体与实参按 `std::decay_t` 存入闭包，类型擦除为 `void()`，与 `std::thread` 一致；
	/// - `Commit` 交回 `std::future<R>`（`CommitDetached` 是不建 future 的热路径）；
	/// - 任务抛出的异常**不会**逃出 worker：它进 future（`get()` 时抛出）、计入
	///   `Stats().TaskExceptions`、并交给可选的异常处理器。
	///
	/// 线程与归属（**读这段再改代码**）：
	/// - 池的 owner 线程 = 构造它的线程；`Shutdown` 与析构只允许它调用（要 join / 销毁 `Thread`
	///   对象，而 `Thread` 的 `StartTask`/`Detach` 本就是 owner-only）；
	/// - `Commit` / `CommitDetached` / `ShrinkToCore` 可从任意线程调用；非 owner 提交只置扩容请求，
	///   由 owner 侧兑现（`ShrinkToCore` 不碰句柄，只读原子量并唤醒 worker）；
	/// - **没有每帧泵**：worker 自决退出（空闲超时 / 显式收缩），owner 侧在 `Commit` /
	///   `Shutdown` / 析构里**懒回收**已退出的 worker。
	/// - worker 内部**禁止**调用引擎单线程设施（`CoroutineScheduler` / `DeferredDestroyQueue` /
	///   `NotificationBox`）：那些组件在 Release 下会抛 `CorePluginThreadViolation`。
	///
	/// 不变量（改动前必读，改完同步更新）：
	///
	///   P1 `m_workers`（槽位表）只由 owner 线程读写；worker 只写自己的原子量、只碰队列
	///   P2 只有 worker 循环从队列取任务；出队、容量判定、收缩/停止判定都在同一个队列锁内完成
	///   P3 worker 的等待必须同时响应四者：有新任务 / 池停止 / 显式收缩 / 线程 stop_token
	///      （漏掉 stop_token ⇒ `~Thread` 的 request_stop 唤不醒它，析构挂死）
	///   P4 任务的异常必须在任务闭包内收口：逃出循环体会让 worker 带上 HasException 死掉，
	///      池只能把它当"worker 意外死亡"（计入 `WorkerCrashes`）
	///   P5 worker 退出路径恰好扣减一次 active 计数：停止/stop_token 路径无条件扣减，
	///      收缩路径经 `TryRetire()`（CAS 保证不会退到低于常驻量）
	///   P6 任务闭包只持有 { promise, 绑定后的可调用体, 异常汇 }，**不捕获池对象**；
	///      因此一次性临时线程可能在池析构之后才跑完，也不会悬垂
	///   P7 扩容决策只在 owner 线程做（只有它能创建 `Thread`）；非 owner 提交只置 `m_growRequested`
	///   P8 队列满策略的动作一律在锁外执行；`Block` 在本池 worker 内退化为 `CallerRuns`
	class ThreadPool final
	{
	public:
		explicit ThreadPool(const ThreadPoolConfig& Config);
		~ThreadPool() noexcept;

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;
		ThreadPool(ThreadPool&&) = delete;
		ThreadPool& operator=(ThreadPool&&) = delete;

		/// @brief 提交任务并取得其 future
		/// @tparam F 任务体（可调用）
		/// @tparam TaskArgs 实参（按 `std::decay_t` 存储）
		/// @return `code` 给出落点；仅 `Accepted`/`ExecutedInline`/`ExecutedOnTemporaryThread` 时 `future` 有效
		template <typename F, typename... TaskArgs>
			requires std::invocable<std::decay_t<F>&, std::decay_t<TaskArgs>...>
		[[nodiscard]] ThreadPoolCommitResult<std::invoke_result_t<std::decay_t<F>&, std::decay_t<TaskArgs>...>>
		Commit(F&& task, TaskArgs&&... args)
		{
			using ResultType = std::invoke_result_t<std::decay_t<F>&, std::decay_t<TaskArgs>...>;

			std::promise<ResultType> promise;
			std::future<ResultType> future = promise.get_future();

			TaskType closure = MakeTaskClosure<ResultType>(
				std::move(promise), std::forward<F>(task), std::forward<TaskArgs>(args)...);

			const ThreadPoolCommitResultCode code = DispatchTask(closure);

			// 拒绝时按约定交回**无效** future（被丢弃的那个 future 随 promise 析构而作废）
			if (code == ThreadPoolCommitResultCode::RejectedQueueFull
				|| code == ThreadPoolCommitResultCode::RejectedStopping)
			{
				return ThreadPoolCommitResult<ResultType>{ code, std::future<ResultType>{} };
			}

			return ThreadPoolCommitResult<ResultType>{ code, std::move(future) };
		}

		/// @brief 提交"发射后不管"的任务（不建 future，热路径零额外分配）
		/// @return 落点结果码（语义同 `Commit`，但没有 `future` 可查）
		template <typename F, typename... TaskArgs>
			requires std::invocable<std::decay_t<F>&, std::decay_t<TaskArgs>...>
		ThreadPoolCommitResultCode CommitDetached(F&& task, TaskArgs&&... args)
		{
			TaskType closure = MakeDetachedClosure(std::forward<F>(task), std::forward<TaskArgs>(args)...);
			return DispatchTask(closure);
		}

		/// @brief 请求把线程收缩到常驻量（**非阻塞**：既不停止线程，也不等待线程结束）
		/// @return true = 请求已受理：当前**正在工作**的 worker 不超过常驻量，因此空闲的超编
		///         worker 会在回到等待时自行退出，最终落到常驻量；
		///         false = 无法收缩到常驻量（正在关停 / 已不超过常驻量 /
		///         正在工作的 worker 多于常驻量 —— 不打断它们就到不了常驻量）
		/// @note 例：常驻 6、当前 12，但正在工作的有 8 个 ⇒ 返回 false
		/// @note 可在**任意线程**调用：它不停止 / 不等待 / 不回收任何线程，只读原子量并唤醒 worker；
		///       之后用 `Stats().ActiveWorkers` 观察，已退出 worker 的回收由 owner 侧下一次
		///       池 API 调用（如 `Commit`）懒完成
		bool ShrinkToCore() noexcept;

		/// @brief 关停：停止受理 → 唤醒 worker → join 全部 worker
		/// @param drainPending true = 让 worker 把队列里已排队的任务跑完；false = 丢弃它们（计入 `Discarded`）
		/// @exception BadThreadOperation 非 owner 线程调用，或在 worker 内调用（自 join）
		/// @note 幂等；返回后 `Stats().ActiveWorkers == 0`
		void Shutdown(bool drainPending = true);

		[[nodiscard]] ThreadPoolStats Stats() const noexcept;
		[[nodiscard]] ThreadPoolConfig Config() const noexcept { return m_config; }
		[[nodiscard]] bool IsOwnerThread() const noexcept { return m_ownerThread.IsOwner(); }
		[[nodiscard]] bool IsStopping() const noexcept { return m_stopping.load(std::memory_order_acquire); }

		/// @brief 当前线程是否是**某个**池的 worker
		/// @note 供"本池 worker 内不得 Block 提交"的退化判定与用户自查
		[[nodiscard]] static bool IsPoolWorker() noexcept { return s_currentPool != nullptr; }

		/// @brief 设置任务异常处理器
		/// @note 处理器在 **worker 线程**（或一次性临时线程）上被调用，因此**不得**触碰引擎单线程设施
		/// @note 异常同时会被写进对应任务的 future，并计入 `Stats().TaskExceptions`
		void SetTaskExceptionHandler(std::function<void(std::exception_ptr)> handler);
	private:
		/// 队列中的任务：类型擦除的 `void()`（闭包内已含 promise 与异常收口）
		using TaskType = std::move_only_function<void()>;

		/// @brief 任务异常汇：由池与每个任务闭包共同持有（P6）
		/// @note 用 shared_ptr 是因为一次性临时线程可能比池活得更久：那时仍要能计数、能调处理器
		struct ErrorSink
		{
			std::atomic<U64> Exceptions{ 0 };
			std::mutex Mutex;
			std::function<void(std::exception_ptr)> Handler;
		};

		/// @brief worker 槽位：只由 owner 线程访问（P1）
		struct WorkerSlot
		{
			std::unique_ptr<Thread> Handle;
		};

		// ---- 非模板内部实现（ThreadPool.cpp）----
		void SpawnWorker();
		void ReapFinishedWorkers() noexcept;
		void EnsureGrowth();
		[[nodiscard]] bool TryRetire() noexcept;
		void WorkerLoop(std::stop_token token) noexcept;
		void StopAndJoinAll(bool drainPending) noexcept;
		static void RunTaskNow(TaskType& task) noexcept;

		/// @brief 用一次性临时线程执行任务
		/// @return true = 已交给一次性线程；false = **任务已在调用线程执行完毕**（分配或建线程失败时退化）
		[[nodiscard]] static bool RunOnTemporaryThread(TaskType& task);

		/// @note 非 `noexcept`：入队/建线程/建闭包都可能抛 `std::bad_alloc` / `std::system_error`，
		///       让它传播给提交方，好过在 `noexcept` 里 `std::terminate`
		[[nodiscard]] ThreadPoolCommitResultCode DispatchTask(TaskType& task);
		[[nodiscard]] ThreadPoolCommitResultCode DispatchQueueFull(TaskType& task);

		/// @brief 任务异常收口：计数 → 取处理器 → 在**当前线程**调用（不触碰池对象，P6）
		/// @note 处理器自身抛出时吞掉（Debug 报告），绝不让它逃出 worker 循环（P4）
		static void ReportTaskException(const std::shared_ptr<ErrorSink>& sink, std::exception_ptr exception) noexcept
		{
			if (sink == nullptr)
				return;

			sink->Exceptions.fetch_add(1, std::memory_order_relaxed);

			std::function<void(std::exception_ptr)> handler;

			{
				std::lock_guard lock{ sink->Mutex };
				handler = sink->Handler;
			}

			if (!handler)
				return;

			try
			{
				handler(std::move(exception));
			}
			catch (...)
			{
				DEBUG_VERIFY_REPORT(false, "ThreadPool: 任务异常处理器自身抛出异常，已吞掉");
			}
		}

		// ---- 任务闭包构造 ----
		/// @brief 任务体与实参绑定（与 std::thread 一致：实参按 decay 存储，调用时以右值传入）
		template <typename F, typename... TaskArgs>
		[[nodiscard]] static auto BindTask(F&& task, TaskArgs&&... args)
		{
			return [task = std::decay_t<F>(std::forward<F>(task)), arguments = std::tuple<std::decay_t<TaskArgs>...>(std::forward<TaskArgs>(args)...)]
				() mutable -> decltype(auto)
			{
				return std::apply(
					[&task](auto&&... unpacked) -> decltype(auto)
					{
						return std::invoke(task, std::move(unpacked)...);
					},
					arguments);
			};
		}

		/// @brief 带 future 的任务闭包：执行 → 填 promise；异常 → 进 future + 计数 + 处理器
		/// @note 闭包只持有 { promise, 绑定体, 异常汇 }，**不捕获池对象**（P6）
		template <typename ResultType, typename F, typename... TaskArgs>
		[[nodiscard]] TaskType MakeTaskClosure(std::promise<ResultType> promise, F&& task, TaskArgs&&... args)
		{
			std::shared_ptr<ErrorSink> sink = m_errorSink;

			return TaskType{ [promise = std::move(promise), bound = BindTask(std::forward<F>(task), std::forward<TaskArgs>(args)...), sink]() mutable
			{
				try
				{
					if constexpr (std::is_void_v<ResultType>)
					{
						std::invoke(bound);
						promise.set_value();
					}
					else
					{
						promise.set_value(std::invoke(bound));
					}
				}
				catch (...)
				{
					const std::exception_ptr exception = std::current_exception();
					promise.set_exception(exception);
					ReportTaskException(sink, exception);
				}
			} };
		}

		/// @brief 不带 future 的任务闭包：异常只计数 + 交给处理器
		template <typename F, typename... TaskArgs>
		[[nodiscard]] TaskType MakeDetachedClosure(F&& task, TaskArgs&&... args)
		{
			std::shared_ptr<ErrorSink> sink = m_errorSink;

			return TaskType{ [bound = BindTask(std::forward<F>(task), std::forward<TaskArgs>(args)...), sink]() mutable
			{
				try
				{
					std::invoke(bound);
				}
				catch (...)
				{
					ReportTaskException(sink, std::current_exception());
				}
			} };
		}

		// ---- 数据 ----
		ThreadPoolConfig m_config;
		OwnerThread m_ownerThread;

		/// 任务队列（跨线程：worker 出队、任意线程入队）
		std::deque<TaskType> m_queue;
		mutable std::mutex m_queueMutex;
		std::condition_variable_any m_queueCv;   ///< worker 等任务；与 stop_token 一同等待（P3）
		std::condition_variable m_spaceCv;       ///< `Block` 策略等空位

		/// worker 槽位表与相关计数：表只由 owner 访问（P1）
		std::vector<WorkerSlot> m_workers;
		std::atomic<U32> m_activeWorkers{ 0 };   ///< 存活 worker（owner 创建时 +1、worker 退出时 -1，P5）
		std::atomic<U32> m_busyWorkers{ 0 };     ///< 正在执行任务的 worker（worker 取到任务时 +1、跑完 -1）
		std::atomic<U32> m_idleWorkers{ 0 };     ///< 正停在队列上等待的 worker
		std::atomic<U32> m_workerSlots{ 0 };     ///< 槽位数（含未回收的已退出者）
		std::atomic<bool> m_growRequested{ false };    ///< 非 owner 提交留下的扩容请求（P7）
		std::atomic<bool> m_shrinkRequested{ false };  ///< 显式收缩请求（owner 在满足后清除）

		std::atomic<bool> m_stopping{ false };
		bool m_drainPending = false;                   ///< 仅 owner 写；worker 读（受队列锁保护）
		bool m_stopped = false;                        ///< 仅 owner

		std::shared_ptr<ErrorSink> m_errorSink;
		std::atomic<U64> m_submitted{ 0 };
		std::atomic<U64> m_completed{ 0 };
		std::atomic<U64> m_rejected{ 0 };
		std::atomic<U64> m_discarded{ 0 };
		std::atomic<U64> m_temporaryThreads{ 0 };
		std::atomic<U64> m_workerCrashes{ 0 };

		/// 当前线程所属的池（worker 循环设置；供 `IsPoolWorker()` 与 Block 退化判定）
		inline static thread_local ThreadPool* s_currentPool = nullptr;
	};
}
