// File /Native/Engine/Core/GlobalThreadPool.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Thread/ThreadPool.hpp"
#include "../Utils/Singleton.hpp"

namespace PenEngine
{
	class GlobalThreadPool : public Singleton<GlobalThreadPool>
	{
	public:
		void Initialize(const ThreadPoolConfig& config)
		{
			m_pool = std::make_unique<ThreadPool>(config);
		}

		template <typename F, typename... TaskArgs>
			requires std::invocable<std::decay_t<F>&, std::decay_t<TaskArgs>...>
		[[nodiscard]] ThreadPoolCommitResult<std::invoke_result_t<std::decay_t<F>&, std::decay_t<TaskArgs>...>>
			Commit(F&& task, TaskArgs&&... args)
		{
			return m_pool->Commit(std::forward<F>(task), std::forward<TaskArgs>(args)...);
		}

		template <typename F, typename... TaskArgs>
			requires std::invocable<std::decay_t<F>&, std::decay_t<TaskArgs>...>
		ThreadPoolCommitResultCode CommitDetached(F&& task, TaskArgs&&... args)
		{
			return m_pool->CommitDetached(std::forward<F>(task), std::forward<TaskArgs>(args)...);
		}

		bool ShrinkToCore() noexcept { return m_pool->ShrinkToCore(); }

		void Shutdown(bool drainPending = true) { return m_pool->Shutdown(drainPending); }

		[[nodiscard]] ThreadPoolStats Stats() const noexcept { return m_pool->Stats(); }
		[[nodiscard]] ThreadPoolConfig Config() const noexcept { return m_pool->Config(); }
		[[nodiscard]] bool IsOwnerThread() const noexcept { return m_pool->IsOwnerThread(); }
		[[nodiscard]] bool IsStopping() const noexcept { return m_pool->IsStopping(); }

		[[nodiscard]] static bool IsPoolWorker() noexcept { return ThreadPool::IsPoolWorker(); }

		void SetTaskExceptionHandler(std::function<void(std::exception_ptr)> handler) { return m_pool->SetTaskExceptionHandler(handler); }
	private:
		std::unique_ptr<ThreadPool> m_pool;
	};
}
