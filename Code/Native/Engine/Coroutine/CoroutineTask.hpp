// File /Native/Engine/Coroutine/CoroutineTask.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "../Exception/Exception.hpp"
#include "ICoroutineInstruction.hpp"

namespace PenEngine
{
	class CoroutineScheduler; // 前置声明：驱动接口只对调度器开放

	/// @brief 协程任务的当前等待状态，由UpdateAwaiterState返回
	enum class CoroutineTaskState
	{
		WaitInstruction,              // 正在等待指令，且指令未就绪
		WaitSubtask,                  // 正在等待子任务，且子任务未完成
		Ready,                        // 可以恢复（任务已完成、尚未挂起，或等待对象已就绪）
		SubtaskHasUnhandledException  // 自身或等待链上的子任务存在未处理异常
	};

	template <typename T>
	class CoroutineTask;

	namespace Detail
	{
		class TaskBase; // 前置声明：PromiseBase的等待对象需要按基类指针记录子任务

		/// @brief promise公共部分：当前等待对象与未处理异常
		template <typename Task>
		class PromiseBase
		{
		public:
			using AwaitedObject = std::variant<ICoroutineInstruction*, TaskBase*>;

			static std::suspend_always initial_suspend() noexcept
			{
				return {};
			}

			static std::suspend_always final_suspend() noexcept
			{
				return {};
			}

			void unhandled_exception() noexcept
			{
				m_unhandledException = std::current_exception();
			}

			/// @brief 获取未处理异常（可能为空）
			[[nodiscard]] std::exception_ptr GetUnhandledException() const noexcept
			{
				return m_unhandledException;
			}

			/// @brief 设置当前等待对象（指令），由ICoroutineInstruction::Awaitable::await_suspend调用
			void SetAwaitedObject(ICoroutineInstruction* instruction) noexcept
			{
				m_awaitedObject = instruction;
			}

			/// @brief 设置当前等待对象（子任务），由TaskCommon::SubTaskAwaitable::await_suspend调用
			void SetAwaitedObject(TaskBase* subTask) noexcept
			{
				m_awaitedObject = subTask;
			}

			[[nodiscard]] AwaitedObject CurrentAwaitedObject() const noexcept
			{
				return m_awaitedObject;
			}

			/// @brief 清空当前等待对象
			void ClearAwaitedObject() noexcept
			{
				m_awaitedObject = static_cast<ICoroutineInstruction*>(nullptr);
			}
		protected:
			AwaitedObject m_awaitedObject;
			std::exception_ptr m_unhandledException;
		};

		/// @brief T != void 的promise：co_return value; 存储返回值
		template <typename T>
		class ValuePromise : public PromiseBase<CoroutineTask<T>>
		{
		public:
			CoroutineTask<T> get_return_object() noexcept
			{
				return CoroutineTask<T>(std::coroutine_handle<ValuePromise>::from_promise(*this));
			}

			void return_value(T value)
			{
				m_result.emplace(std::move(value));
			}

			[[nodiscard]] const T& Result() const
			{
				if (!m_result.has_value())
					ThrowException(Exception("BadCoroutineResult", "任务未正常完成（存在未处理异常），无法获取返回值"));

				return *m_result;
			}
		private:
			std::optional<T> m_result;
		};

		/// @brief T == void 的promise：co_return; 无返回值
		template <>
		class ValuePromise<void> : public PromiseBase<CoroutineTask<void>>
		{
		public:
			CoroutineTask<void> get_return_object() noexcept;

			void return_void() noexcept {}
		};

		enum class CoroutineSubtaskResumeState
		{
			Progressed,   // 实际推进了一步
			Stuck         // 子任务链停在WaitInst/异常上，无法继续驱动
		};

		template <typename Task, typename Promise, typename ValueType>
		class TaskCommon;

		/// @brief 模板擦除类
		/// @note 驱动接口（Resume/ResumeSubTask）是调度器的专属职责，故设为 private：
		///       只对本文件的 TaskCommon（等待链上的父任务驱动子任务）与 CoroutineScheduler 开放。
		///       重入（对执行中的任务再次 resume）由调度侧的调用纪律排除，不再需要运行期执行栈。
		class PENFRAMEWORK_NO_VTABLE TaskBase
		{
			friend class ::PenEngine::CoroutineScheduler;

			template <typename Task, typename Promise, typename ValueType>
			friend class TaskCommon;
		public:
			virtual ~TaskBase() noexcept = default;
			[[nodiscard]] virtual bool Done() const noexcept = 0;
			[[nodiscard]] virtual std::exception_ptr FindUnhandledException() const = 0;
			[[nodiscard]] virtual CoroutineTaskState UpdateAwaiterState() const = 0;
			[[nodiscard]] virtual std::exception_ptr GetUnhandledException() const noexcept = 0;

			/// @brief 当前等待的指令（未等待指令时为nullptr）
			/// 供调度器在drain超时等路径上请求取消外部操作
			[[nodiscard]] virtual ICoroutineInstruction* CurrentAwaitedInstruction() const noexcept = 0;
		private:
			virtual CoroutineSubtaskResumeState ResumeSubTask() = 0;
			virtual void Resume() = 0;
		};

		/// @brief 任务公共机制：句柄生命周期、状态机、子任务驱动
		template <typename Task, typename Promise, typename ValueType>
		class TaskCommon : public TaskBase
		{
		public:
			using CoroutineHandle = std::coroutine_handle<Promise>;
			using GenericCoroutineHandle = std::coroutine_handle<>;
			using AwaitedObject = std::variant<ICoroutineInstruction*, TaskBase*>;

			/// @brief 子任务等待器，co_await CoroutineTask时使用
			/// await_suspend时把子任务注册为父协程的当前等待对象，随后挂起父协程，等待调度器驱动子任务
			struct SubTaskAwaitable
			{
				[[nodiscard]] bool await_ready() const noexcept
				{
					return m_subTask->Done();
				}

				template <typename PromiseType>
				std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> coroutine) const noexcept
				{
					coroutine.promise().SetAwaitedObject(static_cast<TaskBase*>(m_subTask));
					return std::noop_coroutine();
				}

				ValueType await_resume() const
				{
					if constexpr (std::is_void_v<ValueType>)
						return;
					else
						return m_subTask->Result();
				}

				Task* m_subTask = nullptr;
			};

		protected:
			TaskCommon() noexcept = default;
			explicit TaskCommon(CoroutineHandle handle) noexcept : m_coroutineHandle(handle) {}

			virtual ~TaskCommon() noexcept override
			{
				if (m_coroutineHandle)
					m_coroutineHandle.destroy();
			}

			TaskCommon(TaskCommon&& other) noexcept : m_coroutineHandle(std::exchange(other.m_coroutineHandle, nullptr)) {}
			TaskCommon& operator=(TaskCommon&& other) noexcept
			{
				if (&other == this)
					return *this;

				if (m_coroutineHandle)
					m_coroutineHandle.destroy();

				m_coroutineHandle = std::exchange(other.m_coroutineHandle, nullptr);
				return *this;
			}

		public:
			TaskCommon(const TaskCommon&) = delete;
			TaskCommon& operator=(const TaskCommon&) = delete;

			[[nodiscard]] virtual bool Done() const noexcept override
			{
				return m_coroutineHandle == nullptr || m_coroutineHandle.done();
			}

			/// @brief 更新并返回当前等待状态
			/// 自身未处理异常优先上报（抛出异常的任务同时处于Done状态，必须早于Done检查），
			/// 随后通过std::visit分派到AwaiterStateVisitor的两个operator()重载分别检查指令与子任务，
			/// 子任务分支会沿等待链递归下探，将更深层的未处理异常上浮
			[[nodiscard]] virtual CoroutineTaskState UpdateAwaiterState() const override
			{
				if (GetUnhandledException())
					return CoroutineTaskState::SubtaskHasUnhandledException;

				if (Done())
					return CoroutineTaskState::Ready;

				return std::visit(AwaiterStateVisitor{}, m_coroutineHandle.promise().CurrentAwaitedObject());
			}

			SubTaskAwaitable operator co_await() noexcept
			{
				return SubTaskAwaitable{ static_cast<Task*>(this) };
			}
		private:
			/// @brief 恢复协程一步（驱动接口：仅 CoroutineScheduler）
			/// @note 调度器保证只对处于挂起态的任务调用本函数：任务执行期间不会被再次驱动
			virtual void Resume() override
			{
				if (Done())
					return;

				// 恢复即离开当前await点：清空等待对象，
				// 防止后续经不注册自身的awaiter（如suspend_always）挂起时残留悬空指针
				m_coroutineHandle.promise().ClearAwaitedObject();

				m_coroutineHandle.resume();
			}

			/// @brief 恢复当前等待的子任务（驱动接口：CoroutineScheduler 与等待链上的父任务）
			/// 若子任务等待的是更深层的子任务，则递归向下驱动
			/// @retval Progressed 实际推进了某个协程
			/// @retval Stuck 子任务链停在无法推进的指令上
			virtual CoroutineSubtaskResumeState ResumeSubTask() override
			{
				AwaitedObject awaited = m_coroutineHandle.promise().CurrentAwaitedObject();

				TaskBase* subTask = nullptr;
				if (auto* pSubTask = std::get_if<TaskBase*>(&awaited))
					subTask = *pSubTask;

				if (subTask == nullptr || subTask->Done())
					return CoroutineSubtaskResumeState::Stuck;

				switch (subTask->UpdateAwaiterState())
				{
					case CoroutineTaskState::Ready:
						subTask->Resume();
						return CoroutineSubtaskResumeState::Progressed;
					case CoroutineTaskState::WaitSubtask:
						return subTask->ResumeSubTask();
					case CoroutineTaskState::WaitInstruction:
					case CoroutineTaskState::SubtaskHasUnhandledException:
						return CoroutineSubtaskResumeState::Stuck;
				}

				return CoroutineSubtaskResumeState::Stuck;
			}
		public:
			/// @brief 获取自身的未处理异常（可能为空）
			[[nodiscard]] virtual std::exception_ptr GetUnhandledException() const noexcept override
			{
				return m_coroutineHandle ? m_coroutineHandle.promise().GetUnhandledException() : nullptr;
			}

			/// @brief 当前等待的指令（未等待指令时为nullptr）
			[[nodiscard]] virtual ICoroutineInstruction* CurrentAwaitedInstruction() const noexcept override
			{
				if (!m_coroutineHandle)
					return nullptr;

				AwaitedObject awaited = m_coroutineHandle.promise().CurrentAwaitedObject();
				if (auto* pInstruction = std::get_if<ICoroutineInstruction*>(&awaited))
					return *pInstruction;

				return nullptr;
			}

			/// @brief 沿等待链下探，返回实际持有未处理异常的任务的exception_ptr
			/// 自身为空时递归检查子任务，用于调度器在SubTaskHasUnhandledException状态下定位异常本体
			[[nodiscard]] virtual std::exception_ptr FindUnhandledException() const override
			{
				if (auto exception = GetUnhandledException())
					return exception;

				if (Done())
					return nullptr;

				AwaitedObject awaited = m_coroutineHandle.promise().CurrentAwaitedObject();
				if (auto* pSubTask = std::get_if<TaskBase*>(&awaited))
				{
					if (*pSubTask != nullptr)
						return (*pSubTask)->FindUnhandledException();
				}

				return nullptr;
			}
		protected:
			/// @brief 通过两个operator()重载分别检查指令与子任务的等待状态
			struct AwaiterStateVisitor
			{
				CoroutineTaskState operator()(ICoroutineInstruction* instruction) const
				{
					if (instruction == nullptr || instruction->IsReady())
						return CoroutineTaskState::Ready;
					return CoroutineTaskState::WaitInstruction;
				}

				CoroutineTaskState operator()(TaskBase* subTask) const
				{
					if (subTask == nullptr)
						return CoroutineTaskState::Ready;

					// 沿等待链递归下探，将更深层的未处理异常上浮
					if (subTask->UpdateAwaiterState() == CoroutineTaskState::SubtaskHasUnhandledException)
						return CoroutineTaskState::SubtaskHasUnhandledException;

					if (subTask->Done())
						return CoroutineTaskState::Ready;
					return CoroutineTaskState::WaitSubtask;
				}
			};

			CoroutineHandle m_coroutineHandle;
		};
	}

	/// @brief 有返回值的协程任务：co_return value; 完成后通过Result()取回
	/// 例：CoroutineTask<std::expected<T, E>> 通过expected本身判断成败
	template <typename T>
	class CoroutineTask : public Detail::TaskCommon<CoroutineTask<T>, Detail::ValuePromise<T>, T>
	{
	public:
		using promise_type = Detail::ValuePromise<T>;
		using ResultType = T;
		using Base = Detail::TaskCommon<CoroutineTask<T>, promise_type, T>;
		using CoroutineHandle = std::coroutine_handle<promise_type>;

		CoroutineTask() noexcept = default;
		explicit CoroutineTask(CoroutineHandle handle) noexcept : Base(handle) {}
		CoroutineTask(CoroutineTask&&) noexcept = default;
		CoroutineTask& operator=(CoroutineTask&&) noexcept = default;
		CoroutineTask(const CoroutineTask&) = delete;
		CoroutineTask& operator=(const CoroutineTask&) = delete;

		/// @brief 任务完成后的返回值
		/// @exception BadCoroutineResult 任务未正常完成（存在未处理异常）时调用
		[[nodiscard]] const T& Result() const
		{
			if (Base::m_coroutineHandle == nullptr)
				ThrowException(Exception("BadCoroutineResult", "任务未正常完成，无法获取返回值"));
			return Base::m_coroutineHandle.promise().Result();
		}
	};

	/// @brief 无返回值的协程任务：co_return;
	template <>
	class CoroutineTask<void> : public Detail::TaskCommon<CoroutineTask<void>, Detail::ValuePromise<void>, void>
	{
	public:
		using promise_type = Detail::ValuePromise<void>;
		using ResultType = void;
		using Base = Detail::TaskCommon<CoroutineTask<void>, promise_type, void>;
		using CoroutineHandle = std::coroutine_handle<promise_type>;

		CoroutineTask() noexcept = default;
		explicit CoroutineTask(CoroutineHandle handle) noexcept : Base(handle) {}
		CoroutineTask(CoroutineTask&&) noexcept = default;
		CoroutineTask& operator=(CoroutineTask&&) noexcept = default;
		CoroutineTask(const CoroutineTask&) = delete;
		CoroutineTask& operator=(const CoroutineTask&) = delete;
	};

	inline CoroutineTask<void> Detail::ValuePromise<void>::get_return_object() noexcept
	{
		return CoroutineTask<void>(std::coroutine_handle<ValuePromise>::from_promise(*this));
	}
}
