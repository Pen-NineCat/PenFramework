// File /Native/Engine/Coroutine/CoroutineScheduler.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../Exception/Exception.hpp"
#include "../Object/PObject.h"
#include "../Object/SignalObject.hpp"
#include "../Utils/OwnerThread.hpp"
#include "../Utils/ResourceScopeGuard.hpp"
#include "../Utils/Singleton.hpp"
#include "CoroutineTask.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
#include <functional>
#include <plf_hive.h>
#include <thread>
#include <utility>
#include <vector>

namespace PenEngine
{
	class CoreApplication;

	/// @note 仅支持单线程访问，构造时记录所属线程并在入口校验。
	class CoroutineScheduler final : public Singleton<CoroutineScheduler>, SignalObject
	{
		///
		/// 设计取舍：参照 libUV 这类传统 AIO 的做法本应是"请求队列 + 线程池 + 应答队列 + 回调"，
		/// 但那会导致 AIO 与普通协程任务出现两套互不相容的调用链路，
		/// 因此这里统一采用轮询：每帧对每个任务推进一步，
		/// 任务等待的 ICoroutineInstruction 由 IsReady() 自行汇报就绪状态。
		///
		/// 遍历安全与重入纪律（不变量）：
		///   I1 对象迭代器表是"组是否存在"的唯一真相来源：整组回收必须先从它摘除，
		///      此后所有重入路径都看不见这个组
		///   I2 驱动期（m_updateDepth > 0）内绝不擦除 m_groups 或任何子表中的元素，只把槽位置 nullptr；
		///      插入是安全的（hive 元素地址稳定，插入不会使迭代器失效）
		///   I3 任务对象的 delete 只在 Flush 的删除阶段执行，因此永远不会销毁正在执行的协程帧
		///   I4 所有驱动都在 m_updateDepth >= 1 中进行，只有最外层 1 -> 0 才 Flush；
		///      于是"m_updateDepth == 0"等价于"当前没有协程正在执行"
		///   I5 组的存活与两条信号连接同寿：建组时 Connect，回收时 Disconnect；
		///      由 DestroySignal/DestroyLatersSignal 回调直接触发的回收不 Disconnect
		///      （此刻信号正在 Emit 遍历自己的槽位表，回擦自己的槽位是未定义行为）

		friend class CoreApplication;
	public:
		/// @brief 提交归属某个发送者的协程任务，所有权转交给调度器
		/// @param sender 任务归属对象，nullptr 视为无主任务（等价于 PostDetachCoroutine）
		/// @param task 待提交任务
		/// @note 插入前先驱动一次
		template <typename T>
		void PostCoroutineTask(PObject* sender, CoroutineTask<T>&& task);

		/// @brief 提交归属某个发送者的协程任务，所有权转交给调度器
		/// @param sender 任务归属对象，nullptr 视为无主任务（等价于 PostDetachCoroutine）
		/// @param task 待提交任务
		/// @note 插入前先驱动一次，不应该再操作task指针
		void PostCoroutineTask(PObject* sender, Detail::TaskBase* task);

		/// @brief 提交无主协程任务，由调度器内部的无主任务表承载
		template <typename T>
		void PostDetachCoroutine(CoroutineTask<T>&& task);

		/// @brief 当前存活任务数量
		[[nodiscard]] Usize TaskCount() const noexcept
		{
			return m_liveTaskCount;
		}

		/// @brief 判断是否还有存活任务
		[[nodiscard]] bool HasPendingTask() const noexcept
		{
			return m_liveTaskCount != 0;
		}

		/// @brief 判断当前线程是否为调度器所属线程
		[[nodiscard]] bool IsOwnerThread() const noexcept
		{
			return m_ownerThread.IsOwner();
		}

		CoroutineScheduler() noexcept = default;
		virtual ~CoroutineScheduler() noexcept override;

		CoroutineScheduler(const CoroutineScheduler&) = delete;
		CoroutineScheduler& operator=(const CoroutineScheduler&) = delete;
	private:
		using TaskHive = plf::hive<Detail::TaskBase*>;
		using TaskSlot = TaskHive::iterator;

		/// @brief 容器池的迭代器（池的元素类型本身就是 TaskHive，与 TaskSlot 不是同一类型）
		using PoolHive = plf::hive<TaskHive>;
		using PoolSlot = PoolHive::iterator;

		/// @brief 主表元素：一个发送者的全部任务
		struct Group
		{
			PObject* Sender = nullptr;
			TaskHive Tasks;

			Group() = default;
			Group(PObject* sender, TaskHive&& tasks) noexcept : Sender(sender), Tasks(std::move(tasks)) {}
		};

		using MainHive = plf::hive<Group>;
		using MainIter = MainHive::iterator;

		/// @brief 对象迭代器表的值：组在主表中的位置 + 两个组级状态
		struct SenderEntry
		{
			MainIter It{};

			/// 是否已为该发送者占用一次延迟销毁计数（DestroyLater 可能被多次调用，需要幂等）
			bool CaughtDeferred = false;

			/// 发送者是否已析构：置位后不能再解引用 Sender，也不能再断开它的信号
			bool SenderDead = false;
		};

		/// @brief 单步驱动结果
		enum class StepOutcome
		{
			Advanced,   // 本步实际推进了某个协程
			Waiting,    // 本步只能等待（指令未就绪/子任务链停在指令上）
			Finished    // 任务已结束（完成或存在未处理异常），应被回收
		};

		/// @brief 一趟驱动的统计（drain 的预算按任务步数计）
		struct DriveStatistics
		{
			Usize Advanced = 0;
			Usize Waited = 0;
		};

		// ---- 驱动入口：仅 CoreApplication 与本类析构可调用 ----
		void Update();
		void Drain() noexcept;
		void SetExceptionHandler(std::function<void(std::exception_ptr)> handler);

		// ---- 提交 ----
		void PostRaw(PObject* sender, Detail::TaskBase* task);
		MainIter EnsureGroup(PObject* sender, bool& created);
		void AttachTaskToGroup(PObject* sender, Detail::TaskBase* task);
		void ConnectSenderSignals(PObject* sender);

		// ---- 驱动 ----
		DriveStatistics DriveHive(TaskHive& tasks, PObject* sender);
		static StepOutcome DriveTaskStep(Detail::TaskBase* task);

		/// @brief 驱动任务一步
		/// @return true 表示任务已结束（完成或存在未处理异常），应被回收
		static bool DriveTaskOnce(Detail::TaskBase* task);

		// ---- 退役 / 回收 ----
		void MarkDirty(PObject* sender) noexcept;
		void ReleaseGroup(PObject* sender, bool senderDying) noexcept;
		void OnSenderDestroy(PObject* object) noexcept;
		void OnSenderDestroyLater(PObject* object) noexcept;
		void Flush() noexcept;
		void FlushPendingDelete() noexcept;
		void CompactDirty() noexcept;
		void ForceRetireAll() noexcept;

		// ---- drain 辅助 ----
		bool CancelPendingInstructions() noexcept;
		static bool CancelInstructionOf(Detail::TaskBase* task) noexcept;

		void HandleException(std::exception_ptr exception) const;
		void ReportException(Detail::TaskBase* task) const;
		void ReportExceptionSafely(Detail::TaskBase* task) const noexcept;

		void VerifyThread() const;

		// ---- 数据 ----
		MainHive m_groups;
		boost::unordered::unordered_flat_map<PObject*, SenderEntry> m_senderIndex;
		PoolHive m_hivePool;
		TaskHive m_detachedTasks;

		/// 待删除任务：驱动期只入队，删除统一发生在 Flush 的删除阶段（I3）
		std::vector<Detail::TaskBase*> m_pendingDelete;

		/// 本帧有槽位被置空的组（允许重复，压缩时按查表命中判定）
		std::vector<PObject*> m_dirtyGroups;

		/// 无主任务表是否有待压缩的空槽
		bool m_detachedDirty = false;

		Usize m_liveTaskCount = 0;
		Usize m_updateDepth = 0;
		bool m_draining = false;
		std::function<void(std::exception_ptr)> m_exceptionHandler;

		/// @brief 归属线程：单例总是在主线程首次构造，故这里记录的就是主线程
		OwnerThread m_ownerThread;
	};

	template <typename T>
	void CoroutineScheduler::PostCoroutineTask(PObject* sender, CoroutineTask<T>&& task)
	{
		VerifyThread();
		PostRaw(sender, new CoroutineTask<T>(std::move(task)));
	}

	template <typename T>
	void CoroutineScheduler::PostDetachCoroutine(CoroutineTask<T>&& task)
	{
		VerifyThread();
		PostRaw(nullptr, new CoroutineTask<T>(std::move(task)));
	}
}
