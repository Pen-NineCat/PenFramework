// File /Native/Engine/Coroutine/CoroutineScheduler.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "CoroutineScheduler.hpp"

#include "../Core/DeferredDestroyQueue.h"
#include "../DebugTools/DebugVerify.hpp"

#include <thread>
#include <utility>

namespace PenEngine
{
	namespace
	{
		/// @brief drain期间"实际推进"的任务步数上限
		/// yield型任务（例：while(true) co_await suspend_always{}）无法在析构期完成，需要上限强制结束
		constexpr Usize MaxDrainAdvancedSteps = 10000;

		/// @brief drain期间"仅等待、未推进"的任务步数上限
		/// 等待在途IO的任务空转不消耗推进预算，但也不能无限等待，超过后先尝试取消外部操作
		constexpr Usize MaxDrainWaitSteps = 100000;
	}

	void CoroutineScheduler::PostCoroutineTask(PObject* sender, Detail::TaskBase* task)
	{
		VerifyThread();
		PostRaw(sender,task);
	}

	CoroutineScheduler::~CoroutineScheduler() noexcept
	{
		Drain();
	}

	void CoroutineScheduler::VerifyThread() const
	{
		if (std::this_thread::get_id() != m_ownerThread)
			ThrowException(Exception("CoroutineSchedulerThreadViolation", "调度器仅支持单线程访问，请勿跨线程Post/Update"));
	}

	// ------------------------------------------------------------------
	// 提交
	// ------------------------------------------------------------------

	void CoroutineScheduler::PostRaw(PObject* sender, Detail::TaskBase* task)
	{
		if (task == nullptr)
			return;

		// 有主任务必须在首次驱动之前就把组建好并连上信号（I5）。
		// 否则"协程第一步就销毁自己的发送者"这条路径会在回调里找不到组，
		// 首次驱动返回后又会拿已经析构的 sender 去建组（悬垂键 + 往已析构信号里写槽位）
		bool createdGroup = false;
		if (sender != nullptr)
		{
			try
			{
				(void)EnsureGroup(sender, createdGroup);
			}
			catch (...)
			{
				delete task; // 建组失败（分配失败）：任务尚未入任何表，直接销毁
				throw;
			}
		}

		bool attached = false;

		// 首次驱动也放在驱动期内（I4）：这样驱动期间发生的退役/删除一律走延迟路径，
		// 也就不会在协程体还挂在栈上时销毁它的帧
		{
			const ResourceScopeGuard depthGuard([this]() noexcept { --m_updateDepth; });
			++m_updateDepth;

			// 建了组却没挂上任务（一步完成 / 发送者已析构 / 驱动中抛出）时组会空着，
			// 必须标脏交给 Flush 判定回收；异常路径同样要标
			const ResourceScopeGuard groupGuard([this, sender, createdGroup, &attached]() noexcept
			{
				if (createdGroup && !attached)
					MarkDirty(sender);
			});

			bool finished = false;

			try
			{
				finished = DriveTaskOnce(task);
			}
			catch (...)
			{
				// 协程promise侧抛出（例：return_value的移动构造失败）：任务未入表，交给删除阶段
				m_pendingDelete.push_back(task);
				throw;
			}

			if (finished)
			{
				// 一步即完成（或已有未处理异常）：不占用组内槽位
				m_pendingDelete.push_back(task); // 先入队，保证异常处理器抛出时任务也不会泄漏
				ReportException(task);
			}
			else if (sender == nullptr)
			{
				m_detachedTasks.emplace(task);
				++m_liveTaskCount;
				attached = true;
			}
			else
			{
				AttachTaskToGroup(sender, task);
				attached = true;
			}
		}

		if (m_updateDepth == 0)
			Flush();
	}

	CoroutineScheduler::MainIter CoroutineScheduler::EnsureGroup(PObject* sender, bool& created)
	{
		auto indexIt = m_senderIndex.find(sender);

		if (indexIt != m_senderIndex.end())
		{
			created = false;
			return indexIt->second.It;
		}

		created = true;

		// 从容器池取一个子表容器：move 会偷走它的全部容器块（含空闲块），O(1)
		TaskHive tasks;
		if (!m_hivePool.empty())
		{
			const PoolSlot pooled = m_hivePool.begin();
			tasks = std::move(*pooled);
			m_hivePool.erase(pooled); // 擦掉被搬空的壳（析构是空操作）
		}

		const MainIter group = m_groups.emplace(sender, std::move(tasks));

		SenderEntry entry{};
		entry.It = group;

		// DestroyLater 早于本次 Post 的时序：此时补上延迟销毁计数，
		// 否则本帧稍后的 DeferredDestroyQueue::Update 会把对象连同它的协程一起清掉
		if (DeferredDestroyQueue::IsAlive())
		{
			DeferredDestroyQueue& queue = DeferredDestroyQueue::GetInstance();

			if (queue.IsDeferred(sender))
			{
				queue.CatchDeferredObject(sender);
				entry.CaughtDeferred = true;
			}
		}

		// 先登记查表，再连接信号：连接之后 sender 一旦析构，回调一定能找到本组
		m_senderIndex.emplace(sender, entry);
		ConnectSenderSignals(sender);

		return group;
	}

	void CoroutineScheduler::AttachTaskToGroup(PObject* sender, Detail::TaskBase* task)
	{
		auto indexIt = m_senderIndex.find(sender);

		if (indexIt == m_senderIndex.end() || indexIt->second.SenderDead)
		{
			// 发送者在首次驱动期间已析构（或组已被回收）：任务无处安放，交给删除阶段
			m_pendingDelete.push_back(task);
			return;
		}

		indexIt->second.It->Tasks.emplace(task);
		++m_liveTaskCount;
	}

	void CoroutineScheduler::ConnectSenderSignals(PObject* sender)
	{
		DEBUG_VERIFY_REPORT(sender != nullptr, "CoroutineScheduler: 有主任务必须携带发送者");

		Connect(sender->DestroySignal, [this](SignalObject* object) noexcept { OnSenderDestroy(static_cast<PObject*>(object)); });
		Connect(sender->DestroyLaterSignal, [this](PObject* object) noexcept { OnSenderDestroyLater(object); });
	}

	// ------------------------------------------------------------------
	// 驱动
	// ------------------------------------------------------------------

	CoroutineScheduler::StepOutcome CoroutineScheduler::DriveTaskStep(Detail::TaskBase* task)
	{
		switch (task->UpdateAwaiterState())
		{
			case CoroutineTaskState::Ready:
				task->Resume();
				return task->Done() ? StepOutcome::Finished : StepOutcome::Advanced;
			case CoroutineTaskState::WaitSubtask:
				switch (task->ResumeSubTask())
				{
					case Detail::CoroutineSubtaskResumeState::Progressed:
						return task->Done() ? StepOutcome::Finished : StepOutcome::Advanced;
					case Detail::CoroutineSubtaskResumeState::Stuck:
						return StepOutcome::Waiting;
				}
				return StepOutcome::Waiting;
			case CoroutineTaskState::WaitInstruction:
				return StepOutcome::Waiting;
			case CoroutineTaskState::SubtaskHasUnhandledException:
				return StepOutcome::Finished;
		}

		return StepOutcome::Waiting;
	}

	bool CoroutineScheduler::DriveTaskOnce(Detail::TaskBase* task)
	{
		return DriveTaskStep(task) == StepOutcome::Finished;
	}

	CoroutineScheduler::DriveStatistics CoroutineScheduler::DriveHive(TaskHive& tasks, PObject* sender)
	{
		DriveStatistics statistics;

		for (TaskSlot slot = tasks.begin(); slot != tasks.end(); ++slot)
		{
			Detail::TaskBase* task = *slot;

			if (task == nullptr)
				continue; // 本帧已退役的槽位

			const StepOutcome outcome = DriveTaskStep(task);

			if (outcome == StepOutcome::Waiting)
				++statistics.Waited;
			else
				++statistics.Advanced;

			// 驱动期间可能被重入路径退役（例：DestroySignal -> ReleaseGroup 把整组槽位置空），
			// 此时 slot 已为空，不得再触碰 task
			if (*slot == nullptr)
				continue;

			if (outcome != StepOutcome::Finished)
				continue;

			// 只置空，不擦除：擦除会让仍在遍历本表的迭代器失效（I2）。
			// 退役登记先于异常上报：上报会调用用户处理器，它抛出时任务也应已正确退役
			*slot = nullptr;
			m_pendingDelete.push_back(task);
			--m_liveTaskCount;

			if (sender != nullptr)
				MarkDirty(sender);
			else
				m_detachedDirty = true;

			ReportException(task);
		}

		return statistics;
	}

	void CoroutineScheduler::Update()
	{
		VerifyThread();

		if (m_updateDepth != 0)
		{
			// 驱动过程中再次进入驱动，说明调用点位于某个协程体之下。
			// Debug 下立即抓到违规；Release 下退化为安全空操作
			DEBUG_VERIFY_REPORT(false, "CoroutineScheduler: 不允许在驱动过程中重入 Update（协程体内调用 Update）");
			return;
		}

		{
			const ResourceScopeGuard depthGuard([this]() noexcept { --m_updateDepth; });
			++m_updateDepth;

			for (MainIter group = m_groups.begin(); group != m_groups.end(); ++group)
				(void)DriveHive(group->Tasks, group->Sender);

			(void)DriveHive(m_detachedTasks, nullptr);
		}

		Flush();
	}

	// ------------------------------------------------------------------
	// 退役 / 回收
	// ------------------------------------------------------------------

	void CoroutineScheduler::MarkDirty(PObject* sender) noexcept
	{
		m_dirtyGroups.push_back(sender);
	}

	void CoroutineScheduler::OnSenderDestroy(PObject* object) noexcept
	{
		ReleaseGroup(object, true);
	}

	void CoroutineScheduler::OnSenderDestroyLater(PObject* object) noexcept
	{
		auto indexIt = m_senderIndex.find(object);

		if (indexIt == m_senderIndex.end())
			return; // 该对象当前没有任务，不需要为它占用延迟销毁计数

		if (indexIt->second.CaughtDeferred)
			return; // 幂等：DestroyLater 可能被多次调用

		if (!DeferredDestroyQueue::IsAlive())
			return;

		DeferredDestroyQueue::GetInstance().CatchDeferredObject(object);
		indexIt->second.CaughtDeferred = true;
	}

	void CoroutineScheduler::ReleaseGroup(PObject* sender, bool senderDying) noexcept
	{
		auto indexIt = m_senderIndex.find(sender);

		if (indexIt == m_senderIndex.end())
			return;

		// 先置空全部槽位：此后任何重入路径都看不到这些任务
		for (auto& slot : indexIt->second.It->Tasks)
		{
			if (slot == nullptr)
				continue;

			ReportExceptionSafely(slot);
			m_pendingDelete.push_back(slot);
			slot = nullptr;
			--m_liveTaskCount;
		}

		if (senderDying)
			indexIt->second.SenderDead = true;

		MarkDirty(sender);

		// 驱动期内不回收结构、不删除任务，交给最外层的 Flush；
		// 非驱动期则立刻打一趟（删除与回收同样只发生在 Flush 里）
		if (m_updateDepth == 0)
			Flush();
	}

	void CoroutineScheduler::Flush() noexcept
	{
		if (m_updateDepth != 0)
			return; // 只有最外层驱动结束（1 -> 0）时才回收

		// 删除阶段会执行协程帧析构，可能重入回收路径；抬高深度让它们只记账
		const ResourceScopeGuard depthGuard([this]() noexcept { --m_updateDepth; });
		++m_updateDepth;

		FlushPendingDelete();

		// 压缩阶段不执行任何用户代码，可安全擦除与回收
		CompactDirty();
	}

	void CoroutineScheduler::FlushPendingDelete() noexcept
	{
		// 反复取尾：删除会执行用户代码，可能继续向队尾追加（含本次删除引发的连锁退役）
		while (!m_pendingDelete.empty())
		{
			Detail::TaskBase* task = m_pendingDelete.back();
			m_pendingDelete.pop_back();

			if (task != nullptr)
				delete task; // 虚析构销毁协程帧与其内部挂起的子任务
		}
	}

	void CoroutineScheduler::CompactDirty() noexcept
	{
		Usize index = 0;

		while (index < m_dirtyGroups.size())
		{
			PObject* sender = m_dirtyGroups[index++];

			auto indexIt = m_senderIndex.find(sender);

			if (indexIt == m_senderIndex.end())
				continue; // 组已被回收（重复登记）

			const bool senderDead = indexIt->second.SenderDead;
			const bool caughtDeferred = indexIt->second.CaughtDeferred;
			const MainIter group = indexIt->second.It;
			TaskHive& tasks = group->Tasks;

			// 压缩空槽：只有在空槽清干净后才能判定"组空"
			for (TaskSlot slot = tasks.begin(); slot != tasks.end(); )
			{
				if (*slot == nullptr)
					slot = tasks.erase(slot);
				else
					++slot;
			}

			if (!tasks.empty())
				continue;

			// 整组回收：先摘查表，此后再无任何路径能找到这个组（I1）
			m_senderIndex.erase(indexIt);

			// 容器块（含空闲块）归还容器池，与发送者是否已经析构无关
			m_hivePool.emplace(std::move(tasks));

			if (!senderDead)
			{
				// 组存活与信号连接同寿（I5）
				Disconnect(sender->DestroySignal);
				Disconnect(sender->DestroyLaterSignal);

				// 释放本组占用的延迟销毁计数
				if (caughtDeferred && DeferredDestroyQueue::IsAlive())
					DeferredDestroyQueue::GetInstance().ReleaseDeferredObject(sender);
			}
			// 发送者已析构时既不回擦信号槽位、也不递减延迟销毁计数：
			// 宁可让队列留一个永不为0的计数（泄漏），也不要对已析构对象再走一次
			// "计数归零 -> delete"（双重delete）

			m_groups.erase(group);
		}

		m_dirtyGroups.clear();

		if (m_detachedDirty)
		{
			for (TaskSlot slot = m_detachedTasks.begin(); slot != m_detachedTasks.end(); )
			{
				if (*slot == nullptr)
					slot = m_detachedTasks.erase(slot);
				else
					++slot;
			}

			m_detachedDirty = false;
		}
	}

	void CoroutineScheduler::ForceRetireAll() noexcept
	{
		for (MainIter group = m_groups.begin(); group != m_groups.end(); ++group)
		{
			bool anyRetired = false;

			for (auto& slot : group->Tasks)
			{
				if (slot == nullptr)
					continue;

				ReportExceptionSafely(slot);
				m_pendingDelete.push_back(slot);
				slot = nullptr;
				--m_liveTaskCount;
				anyRetired = true;
			}

			if (anyRetired)
				MarkDirty(group->Sender);
		}

		bool anyDetachedRetired = false;

		for (auto& slot : m_detachedTasks)
		{
			if (slot == nullptr)
				continue;

			ReportExceptionSafely(slot);
			m_pendingDelete.push_back(slot);
			slot = nullptr;
			--m_liveTaskCount;
			anyDetachedRetired = true;
		}

		if (anyDetachedRetired)
			m_detachedDirty = true;
	}

	// ------------------------------------------------------------------
	// Drain
	// ------------------------------------------------------------------

	void CoroutineScheduler::Drain() noexcept
	{
		// 析构/退出路径：noexcept，因此不做线程校验（校验失败会抛异常，进 noexcept 就是 terminate）
		if (m_draining)
		{
			DEBUG_VERIFY_REPORT(false, "CoroutineScheduler: 禁止嵌套 Drain");
			return;
		}

		// m_updateDepth != 0 说明此刻有协程正在执行（I4），销毁它的帧是未定义行为，只能放弃
		if (m_updateDepth != 0)
		{
			DEBUG_VERIFY_REPORT(false, "CoroutineScheduler: 有协程正在执行时不允许 Drain");
			return;
		}

		m_draining = true;
		const ResourceScopeGuard drainingGuard([this]() noexcept { m_draining = false; });

		Usize advancedSteps = 0;
		Usize waitSteps = 0;
		bool cancelRequested = false;

		try
		{
			while (m_liveTaskCount != 0)
			{
				DriveStatistics statistics;

				{
					const ResourceScopeGuard depthGuard([this]() noexcept { --m_updateDepth; });
					++m_updateDepth;

					for (MainIter group = m_groups.begin(); group != m_groups.end(); ++group)
					{
						const DriveStatistics part = DriveHive(group->Tasks, group->Sender);
						statistics.Advanced += part.Advanced;
						statistics.Waited += part.Waited;
					}

					const DriveStatistics detached = DriveHive(m_detachedTasks, nullptr);
					statistics.Advanced += detached.Advanced;
					statistics.Waited += detached.Waited;
				}

				Flush();

				if (m_liveTaskCount == 0)
					break;

				if (statistics.Advanced != 0)
				{
					advancedSteps += statistics.Advanced;
					waitSteps = 0;
				}
				else if (statistics.Waited != 0)
				{
					waitSteps += statistics.Waited;
					// 让出时间片，避免在等待外部操作落地时把CPU全部烧在轮询上
					std::this_thread::yield();
				}
				else
				{
					// 既没有推进也没有等待：没有任何可驱动的状态，直接进入预算分支
					waitSteps = MaxDrainWaitSteps + 1;
				}

				if (advancedSteps <= MaxDrainAdvancedSteps && waitSteps <= MaxDrainWaitSteps)
					continue;

				if (cancelRequested)
					break; // 已经给过取消机会仍无法结束 -> 强杀

				if (!CancelPendingInstructions())
					break; // 没有任何可取消的在途操作（例：while(true) co_await suspend_always{}）-> 强杀

				// 取消请求已发出：重置等待预算，给被取消的操作留出落地时间
				cancelRequested = true;
				waitSteps = 0;
			}
		}
		catch (...)
		{
			// 异常处理器自身抛出：吞掉，继续走强制收尾
		}

		// 兜底：强杀所有残留任务（含未实现 Cancel 的在途操作）
		ForceRetireAll();
		Flush();

		m_dirtyGroups.clear();
		m_pendingDelete.clear();
		m_detachedDirty = false;
		m_hivePool.clear();
	}

	bool CoroutineScheduler::CancelInstructionOf(Detail::TaskBase* task) noexcept
	{
		ICoroutineInstruction* instruction = task->CurrentAwaitedInstruction();

		if (instruction == nullptr)
			return false;

		try
		{
			return instruction->Cancel();
		}
		catch (...)
		{
			return false;
		}
	}

	bool CoroutineScheduler::CancelPendingInstructions() noexcept
	{
		bool requested = false;

		for (MainIter group = m_groups.begin(); group != m_groups.end(); ++group)
		{
			for (auto& slot : group->Tasks)
			{
				if (slot == nullptr)
					continue;

				requested = CancelInstructionOf(slot) || requested;
			}
		}

		for (auto& slot : m_detachedTasks)
		{
			if (slot == nullptr)
				continue;

			requested = CancelInstructionOf(slot) || requested;
		}

		return requested;
	}

	// ------------------------------------------------------------------
	// 异常
	// ------------------------------------------------------------------

	void CoroutineScheduler::SetExceptionHandler(std::function<void(std::exception_ptr)> handler)
	{
		m_exceptionHandler = std::move(handler);
	}

	void CoroutineScheduler::HandleException(std::exception_ptr exception) const
	{
		if (!exception)
			return;

		if (m_exceptionHandler)
			m_exceptionHandler(exception);
	}

	void CoroutineScheduler::ReportException(Detail::TaskBase* task) const
	{
		if (task == nullptr)
			return;

		HandleException(task->FindUnhandledException());
	}

	void CoroutineScheduler::ReportExceptionSafely(Detail::TaskBase* task) const noexcept
	{
		try
		{
			ReportException(task);
		}
		catch (...)
		{
		}
	}
}
