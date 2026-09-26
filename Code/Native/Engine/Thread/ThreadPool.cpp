// File /Native/Engine/Thread/ThreadPool.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "ThreadPool.hpp"
#include <algorithm>
#include <system_error>

namespace PenEngine
{
	namespace
	{
		/// @brief 队列是否还有空位（容量判定；必须在队列锁内调用，P2）
		[[nodiscard]] bool HasQueueSpace(Usize queueSize, Usize maxQueueSize) noexcept
		{
			return queueSize < maxQueueSize;
		}
	}

	ThreadPool::ThreadPool(const ThreadPoolConfig& config)
		: m_config(config)
		, m_errorSink(std::make_shared<ErrorSink>())
	{
		// ---- 参数收敛（Debug 报告，Release 静默修正）----
		const U32 hardware = std::max<U32>(1, Thread::HardwareConcurrency());

		if (m_config.MaxThreads == 0)
			m_config.MaxThreads = hardware;

		// CoreThreads == 0 是合法配置：不预建常驻线程，按需扩容、空闲时允许降到 0（故不报告）

		if (m_config.CoreThreads > m_config.MaxThreads)
		{
			DEBUG_VERIFY_REPORT(false, "ThreadPool: CoreThreads 大于 MaxThreads，已收敛为 MaxThreads");
			m_config.CoreThreads = m_config.MaxThreads;
		}

		if (m_config.MaxQueueSize == 0 && m_config.FullPolicy == ThreadPoolQueueFullPolicy::Block)
		{
			// 容量 0 时队列永远没有空位 ⇒ Block 永不满足，必须收敛，否则提交方会永久阻塞
			DEBUG_VERIFY_REPORT(false, "ThreadPool: maxQueueSize 为 0 时 Block 策略永不满足，已收敛为 Reject");
			m_config.FullPolicy = ThreadPoolQueueFullPolicy::Reject;
		}

		// ---- 预建常驻 worker ----
		for (U32 index = 0; index < m_config.CoreThreads; ++index)
			SpawnWorker();
	}

	ThreadPool::~ThreadPool() noexcept
	{
		if (m_stopped)
			return;

		if (s_currentPool == this)
		{
			// 在 worker 内析构本池：不可能 join 自己 ⇒ 此处只做停止与唤醒，worker 靠 detach 收场
			DEBUG_VERIFY_REPORT(false, "ThreadPool: 不得在 worker 内析构本池（无法 join 自身）");
		}

		// 析构不做 drain：丢弃未开始的任务 + join 在途任务（有界，只等正在跑的那些）
		StopAndJoinAll(false);
	}

	// ================================================================
	// worker 生命周期（全部在 owner 线程）
	// ================================================================

	void ThreadPool::SpawnWorker()
	{
		WorkerSlot slot;
		slot.Handle = std::make_unique<Thread>(Thread::DeferredStart, [this](std::stop_token token) noexcept
		{
			WorkerLoop(token);
		});

		// P5：active 计数由 owner 在创建时 +1（先加后开闸，避免 Stats 短暂少算）
		m_activeWorkers.fetch_add(1, std::memory_order_relaxed);
		m_workerSlots.fetch_add(1, std::memory_order_relaxed);

		if (!slot.Handle->StartTask())
		{
			// 逻辑上不可达（新建对象的启动决策必为 Pending）
			DEBUG_VERIFY_REPORT(false, "ThreadPool::SpawnWorker: StartTask 失败");
			m_activeWorkers.fetch_sub(1, std::memory_order_relaxed);
			m_workerSlots.fetch_sub(1, std::memory_order_relaxed);
			return;
		}

		m_workers.push_back(std::move(slot));
	}

	void ThreadPool::ReapFinishedWorkers() noexcept
	{
		// 只由 owner 调用（P1）：join 并销毁所有已落终态的 worker
		for (auto iterator = m_workers.begin(); iterator != m_workers.end(); )
		{
			Thread* handle = iterator->Handle.get();
			const ThreadTaskResult result = handle->TaskResult();

			if (result == ThreadTaskResult::Running || result == ThreadTaskResult::Waiting)
			{
				++iterator;
				continue;
			}

			if (result == ThreadTaskResult::HasException)
			{
				// P4：任务异常本应在闭包内收口，走到这里说明有东西逃出了 worker 循环
				m_workerCrashes.fetch_add(1, std::memory_order_relaxed);
				DEBUG_VERIFY_REPORT(false, "ThreadPool: worker 意外死亡（异常逃出了任务闭包）");
			}

			(void)handle->Wait();   // 已终态：join（不阻塞）

			iterator = m_workers.erase(iterator);
			m_workerSlots.fetch_sub(1, std::memory_order_relaxed);
		}
	}

	void ThreadPool::EnsureGrowth()
	{
		// 只由 owner 调用（P7）
		if (m_stopping.load(std::memory_order_acquire))
			return;

		// 收缩请求满足后清除：否则新扩容出来的空闲 worker 会在刚变空闲时又立刻退出
		if (m_shrinkRequested.load(std::memory_order_relaxed)
			&& m_activeWorkers.load(std::memory_order_relaxed) <= m_config.CoreThreads)
		{
			m_shrinkRequested.store(false, std::memory_order_relaxed);
		}

		Usize queued = 0;

		{
			std::lock_guard lock{ m_queueMutex };
			queued = m_queue.size();
		}

		// 队列里有任务、且空闲 worker 不够分 ⇒ 补到上限（最多补到 MaxThreads）
		while (queued > m_idleWorkers.load(std::memory_order_relaxed)
			&& m_activeWorkers.load(std::memory_order_relaxed) < m_config.MaxThreads)
		{
			SpawnWorker();
		}
	}

	// ================================================================
	// worker 主体
	// ================================================================

	bool ThreadPool::TryRetire() noexcept
	{
		// P5：只有"超出常驻量"时才允许退出；CAS 保证多个空闲 worker 同时退出也不会低于常驻量
		U32 current = m_activeWorkers.load(std::memory_order_relaxed);

		while (current > m_config.CoreThreads)
		{
			if (m_activeWorkers.compare_exchange_weak(current, current - 1, std::memory_order_relaxed))
				return true;
		}

		return false;
	}

	void ThreadPool::WorkerLoop(std::stop_token token) noexcept
	{
		s_currentPool = this;

		while (true)
		{
			TaskType task;
			bool timedOut = false;

			{
				std::unique_lock lock{ m_queueMutex };

				m_idleWorkers.fetch_add(1, std::memory_order_relaxed);

				// P3：四个唤醒源 —— 有新任务 / 池停止 / 显式收缩 / 线程 stop_token（~Thread 的 request_stop）
				const auto predicate = [this, &token]() noexcept
				{
					return m_stopping.load(std::memory_order_relaxed)
						|| m_shrinkRequested.load(std::memory_order_relaxed)
						|| token.stop_requested()
						|| !m_queue.empty();
				};

				timedOut = !m_queueCv.wait_for(lock, token, m_config.KeepAlive, predicate);

				m_idleWorkers.fetch_sub(1, std::memory_order_relaxed);

				const bool stopping = m_stopping.load(std::memory_order_relaxed);
				const bool stopRequested = token.stop_requested();

				if (!m_queue.empty() && !stopRequested && (!stopping || m_drainPending))
				{
					task = std::move(m_queue.front());
					m_queue.pop_front();
					m_spaceCv.notify_one();   // 让等空位的提交方继续
				}
				else if (stopping || stopRequested)
				{
					// P5：停止 / stop_token 路径无条件扣减 active 后退出
					m_activeWorkers.fetch_sub(1, std::memory_order_relaxed);
					break;
				}
				else if (((timedOut && m_config.AutomaticallyReleaseTimeoutThread)
						|| m_shrinkRequested.load(std::memory_order_relaxed)) && TryRetire())
				{
					// P5：收缩路径由 TryRetire 完成扣减（不会退到低于常驻量）
					break;
				}
				else
				{
					continue;
				}
			}

			// 队列锁已释放：闭包内自带异常收口，跑完计数（P4）
			m_busyWorkers.fetch_add(1, std::memory_order_relaxed);
			task();
			m_busyWorkers.fetch_sub(1, std::memory_order_relaxed);
			m_completed.fetch_add(1, std::memory_order_relaxed);
		}

		s_currentPool = nullptr;
	}

	// ================================================================
	// 提交
	// ================================================================

	void ThreadPool::RunTaskNow(TaskType& task) noexcept
	{
		task();   // 闭包自带异常收口
	}

	bool ThreadPool::RunOnTemporaryThread(TaskType& task)
	{
		// 用 shared_ptr 包一层：Thread 构造失败时（内核资源不足）任务内容仍在，可在本线程就地执行
		std::shared_ptr<TaskType> shared;

		try
		{
			shared = std::make_shared<TaskType>(std::move(task));
		}
		catch (...)
		{
			// 分配失败：task 尚未被移走，调用线程就地执行
			RunTaskNow(task);
			return false;
		}

		try
		{
			// 一次性线程由**提交线程**构造（它的 owner 就是本线程），随即 Detach + 销毁对象：
			// 不占 MaxThreads 名额、不需要池回收；Detach 后销毁是安全的（Thread 的 T8 握手）
			Thread transient{ [shared]() noexcept { (*shared)(); } };

			transient.Detach();
			return true;
		}
		catch (...)
		{
			RunTaskNow(*shared);   // 线程创建失败：内容在 shared 里，就地执行
			return false;
		}
	}

	ThreadPoolCommitResultCode ThreadPool::DispatchQueueFull(TaskType& task)
	{
		switch (m_config.FullPolicy)
		{
			case ThreadPoolQueueFullPolicy::CallerRuns:
				RunTaskNow(task);
				return ThreadPoolCommitResultCode::ExecutedInline;

			case ThreadPoolQueueFullPolicy::TemporaryThread:
				// true = 已交给一次性线程；false = 已在本线程就地执行（两种情况任务都已安排妥当）
				if (RunOnTemporaryThread(task))
				{
					m_temporaryThreads.fetch_add(1, std::memory_order_relaxed);
					return ThreadPoolCommitResultCode::ExecutedOnTemporaryThread;
				}
				return ThreadPoolCommitResultCode::ExecutedInline;

			case ThreadPoolQueueFullPolicy::Block:
			{
				// P8：在本池 worker 内 Block 会与本池互锁（所有 worker 都在等空位、没人消费）。
				// 这是"策略在此上下文不可用"而非契约违例，故静默退化为 CallerRuns（文档已注明）
				if (s_currentPool == this)
				{
					RunTaskNow(task);
					return ThreadPoolCommitResultCode::ExecutedInline;
				}

				std::unique_lock lock{ m_queueMutex };

				m_spaceCv.wait(lock, [this]() noexcept
				{
					return m_stopping.load(std::memory_order_relaxed)
						|| HasQueueSpace(m_queue.size(), m_config.MaxQueueSize);
				});

				if (m_stopping.load(std::memory_order_relaxed))
					return ThreadPoolCommitResultCode::RejectedStopping;

				m_queue.push_back(std::move(task));
				lock.unlock();

				m_queueCv.notify_one();
				m_submitted.fetch_add(1, std::memory_order_relaxed);
				return ThreadPoolCommitResultCode::Accepted;
			}

			case ThreadPoolQueueFullPolicy::Reject:
			default:
				return ThreadPoolCommitResultCode::RejectedQueueFull;
		}
	}

	ThreadPoolCommitResultCode ThreadPool::DispatchTask(TaskType& task)
	{
		m_submitted.fetch_add(1, std::memory_order_relaxed);

		if (m_stopping.load(std::memory_order_acquire))
		{
			m_rejected.fetch_add(1, std::memory_order_relaxed);
			return ThreadPoolCommitResultCode::RejectedStopping;
		}

		const bool isOwner = IsOwnerThread();

		if (isOwner)
			ReapFinishedWorkers();   // 懒回收（顺便让 active 计数与槽位表一致）

		// ---- 尝试入队：容量判定与入队在同一把锁内（P2）----
		bool enqueued = false;

		{
			std::lock_guard lock{ m_queueMutex };

			if (m_stopping.load(std::memory_order_relaxed))
			{
				m_rejected.fetch_add(1, std::memory_order_relaxed);
				return ThreadPoolCommitResultCode::RejectedStopping;
			}

			if (HasQueueSpace(m_queue.size(), m_config.MaxQueueSize))
			{
				m_queue.push_back(std::move(task));
				enqueued = true;
			}
		}

		if (enqueued)
		{
			m_queueCv.notify_one();

			if (isOwner)
				EnsureGrowth();
			else
				m_growRequested.store(true, std::memory_order_relaxed);   // P7：交由 owner 兑现

			return ThreadPoolCommitResultCode::Accepted;
		}

		// ---- 队列满：策略处理（锁外，P8）----
		const ThreadPoolCommitResultCode code = DispatchQueueFull(task);

		if (code == ThreadPoolCommitResultCode::RejectedQueueFull)
			m_rejected.fetch_add(1, std::memory_order_relaxed);

		return code;
	}

	// ================================================================
	// 收缩 / 关停 / 统计
	// ================================================================

	bool ThreadPool::ShrinkToCore() noexcept
	{
		// 允许任意线程调用：本函数不停止线程、不等待线程结束、也不回收 worker，
		// 只读原子量 + 发布收缩意图并唤醒 worker（因此线程安全）

		if (m_stopping.load(std::memory_order_acquire))
			return false;

		if (m_activeWorkers.load(std::memory_order_relaxed) <= m_config.CoreThreads)
			return false;   // 已在常驻量，无可收缩（容量最小即常驻量，不存在更小的场景）

		// 判据：正在工作的 worker 是否已多于常驻量 —— 多于就无法在不打断它们的前提下收缩。
		// 例：常驻 6、当前 12、正在工作 8 ⇒ false（本函数不停止线程、不等待线程结束）
		if (m_busyWorkers.load(std::memory_order_relaxed) > m_config.CoreThreads)
			return false;

		// 只发布意图并唤醒：空闲的超编 worker 会在回到等待时自行退出
		m_shrinkRequested.store(true, std::memory_order_release);
		m_queueCv.notify_all();

		return true;
	}

	void ThreadPool::StopAndJoinAll(bool drainPending) noexcept
	{
		{
			std::lock_guard lock{ m_queueMutex };

			m_drainPending = drainPending;

			if (!drainPending)
			{
				// 丢弃未开始的任务
				m_discarded.fetch_add(m_queue.size(), std::memory_order_relaxed);
				m_queue.clear();
			}
		}

		m_stopping.store(true, std::memory_order_release);
		m_queueCv.notify_all();
		m_spaceCv.notify_all();

		for (WorkerSlot& slot : m_workers)
		{
			if (slot.Handle == nullptr)
				continue;

			// 只用池级停止标志 + notify 唤醒 worker：**不要**在这里调 RequestStop()，
			// 那会让 worker 的 stop_token 立刻被请求，从而在 drain 模式下提前退出、丢下排队任务
			(void)slot.Handle->Wait();
		}

		// 防御：若仍有任务没被取走（drain 期间 worker 提前退出、或本来就是丢弃模式），
		// 在这里清空并计数 —— 销毁闭包会让对应 future 变成 broken_promise，而不是永远 pending
		{
			std::lock_guard lock{ m_queueMutex };
			m_discarded.fetch_add(m_queue.size(), std::memory_order_relaxed);
			m_queue.clear();
		}

		m_workers.clear();
		m_activeWorkers.store(0, std::memory_order_relaxed);
		m_idleWorkers.store(0, std::memory_order_relaxed);
		m_workerSlots.store(0, std::memory_order_relaxed);
		m_stopped = true;
	}

	void ThreadPool::Shutdown(bool drainPending)
	{
		PENFRAMEWORK_VERIFY_OWNER_THREAD_OR_THROW(m_ownerThread,
			"ThreadPool::Shutdown: 只允许在 owner 线程（构造本池的线程）调用",
			BadThreadOperation, "Shutdown", "只允许在 owner 线程调用");

		// 在池内关停本池会在 join 自己所在的 worker 时死锁
		DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(s_currentPool != this,
			"ThreadPool::Shutdown: 不允许在 worker 内关停本池（自 join 死锁）",
			BadThreadOperation, "Shutdown", "不允许在 worker 内关停本池");

		if (m_stopped)
			return;   // 幂等

		StopAndJoinAll(drainPending);
	}

	void ThreadPool::SetTaskExceptionHandler(std::function<void(std::exception_ptr)> handler)
	{
		std::lock_guard lock{ m_errorSink->Mutex };
		m_errorSink->Handler = std::move(handler);
	}

	ThreadPoolStats ThreadPool::Stats() const noexcept
	{
		ThreadPoolStats snapshot;

		snapshot.ActiveWorkers = m_activeWorkers.load(std::memory_order_relaxed);
		snapshot.BusyWorkers = m_busyWorkers.load(std::memory_order_relaxed);
		snapshot.IdleWorkers = m_idleWorkers.load(std::memory_order_relaxed);
		snapshot.WorkerSlots = m_workerSlots.load(std::memory_order_relaxed);

		{
			std::lock_guard lock{ m_queueMutex };
			snapshot.QueuedTasks = m_queue.size();
		}

		snapshot.Submitted = m_submitted.load(std::memory_order_relaxed);
		snapshot.Completed = m_completed.load(std::memory_order_relaxed);
		snapshot.Rejected = m_rejected.load(std::memory_order_relaxed);
		snapshot.Discarded = m_discarded.load(std::memory_order_relaxed);
		snapshot.TaskExceptions = m_errorSink->Exceptions.load(std::memory_order_relaxed);
		snapshot.TemporaryThreadsSpawned = m_temporaryThreads.load(std::memory_order_relaxed);
		snapshot.WorkerCrashes = m_workerCrashes.load(std::memory_order_relaxed);

		return snapshot;
	}
}
