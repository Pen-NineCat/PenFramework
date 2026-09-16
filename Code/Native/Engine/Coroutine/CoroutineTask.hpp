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
		/// @brief promise公共部分：当前等待对象与未处理异常
		template <typename Task>
		class PromiseBase
		{
		public:
			using AwaitedObject = std::variant<ICoroutineInstruction*, Task*>;

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
			void SetAwaitedObject(Task* subTask) noexcept
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
			Stuck,        // 子任务链停在WaitInst/异常上，无法继续驱动
			SubExecuting  // 链上的子任务正在执行中（重入），本次无法驱动
		};

		/// @brief 模板擦除类
		class PENFRAMEWORK_NO_VTABLE TaskBase
		{
		public:
			virtual ~TaskBase() noexcept = default;
			[[nodiscard]] virtual bool Done() const noexcept = 0;
			[[nodiscard]] virtual std::exception_ptr FindUnhandledException() const = 0;
			virtual CoroutineSubtaskResumeState ResumeSubTask() = 0;
			virtual void Resume() = 0;
			[[nodiscard]] virtual CoroutineTaskState UpdateAwaiterState() const = 0;
			[[nodiscard]] virtual std::exception_ptr GetUnhandledException() const noexcept = 0;
		};

		struct ExecutionGuard; // 前置声明

		/// @brief 当前正在C++栈上执行的协程任务链（thread_local）
		/// 所有resume都经由TaskCommon::Resume，故该链完整覆盖所有执行中的任务
		inline thread_local ExecutionGuard* ExecutingTaskHead = nullptr;

		/// @brief 协程执行栈守卫（RAII）
		/// 用于让调度器在析构drain时识别"正在运行"的任务，避免resume运行中的协程（UB+死锁）
		struct ExecutionGuard
		{
			ExecutionGuard* Prev = nullptr;
			TaskBase* Task = nullptr;

			explicit ExecutionGuard(TaskBase* task) noexcept
				: Prev(ExecutingTaskHead), Task(task)
			{
				ExecutingTaskHead = this;
			}

			~ExecutionGuard() noexcept
			{
				ExecutingTaskHead = Prev;
			}
		};

		/// @brief 判断任务是否正在执行（位于调用栈上）
		[[nodiscard]] inline bool IsTaskExecuting(TaskBase* task) noexcept
		{
			for (ExecutionGuard* guard = ExecutingTaskHead; guard != nullptr; guard = guard->Prev)
			{
				if (guard->Task == task)
					return true;
			}
			return false;
		}

		/// @brief 任务公共机制：句柄生命周期、状态机、子任务驱动
		template <typename Task, typename Promise, typename ValueType>
		class TaskCommon : public TaskBase
		{
		public:
			using CoroutineHandle = std::coroutine_handle<Promise>;
			using GenericCoroutineHandle = std::coroutine_handle<>;
			using AwaitedObject = std::variant<ICoroutineInstruction*, Task*>;

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
					coroutine.promise().SetAwaitedObject(m_subTask);
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

			virtual void Resume() override
			{
				if (IsTaskExecuting(this))
					return; // 防止重入resume（嵌套drain经ResumeSubTask的路径可能撞上执行中的任务）

				if (!Done())
				{
					ExecutionGuard guard(this);

					// 恢复即离开当前await点：清空等待对象，
					// 防止后续经不注册自身的awaiter（如suspend_always）挂起时残留悬空指针
					m_coroutineHandle.promise().ClearAwaitedObject();

					m_coroutineHandle.resume();
				}
			}

			/// @brief 恢复当前等待的子任务
			/// 若子任务等待的是更深层的子任务，则递归向下驱动
			/// @retval Progressed 实际推进了某个协程
			/// @retval Stuck 子任务链停在无法推进的指令上
			/// @retval SubExecuting 链上的子任务正在执行中（重入），不能resume
			virtual CoroutineSubtaskResumeState ResumeSubTask() override
			{
				AwaitedObject awaited = m_coroutineHandle.promise().CurrentAwaitedObject();

				Task* subTask = nullptr;
				if (auto* pSubTask = std::get_if<Task*>(&awaited))
					subTask = *pSubTask;

				if (subTask == nullptr || subTask->Done())
					return CoroutineSubtaskResumeState::Stuck;

				switch (subTask->UpdateAwaiterState())
				{
					case CoroutineTaskState::Ready:
						if (IsTaskExecuting(subTask))
							return CoroutineSubtaskResumeState::SubExecuting;
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

			/// @brief 获取自身的未处理异常（可能为空）
			[[nodiscard]] virtual std::exception_ptr GetUnhandledException() const noexcept override
			{
				return m_coroutineHandle ? m_coroutineHandle.promise().GetUnhandledException() : nullptr;
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
				if (auto* pSubTask = std::get_if<Task*>(&awaited))
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

				CoroutineTaskState operator()(Task* subTask) const
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
