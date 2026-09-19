// File /Native/Engine/Utils/ResourceScopeGuard.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <utility>

namespace PenEngine
{
	/// @brief 作用域资源守卫：析构时执行给定清理动作，可被移动，可被提前Dismiss
	/// @note 析构保证不抛出（清理动作抛出时直接终止），避免在栈展开途中二次抛出
	template <typename F>
	class ResourceScopeGuard
	{
	public:
		explicit ResourceScopeGuard(F&& func) noexcept(std::is_nothrow_move_constructible_v<F>)
			: m_func(std::forward<F>(func)), m_active(true)
		{}

		~ResourceScopeGuard() noexcept
		{
			Run();
		}

		ResourceScopeGuard(const ResourceScopeGuard&) = delete;
		ResourceScopeGuard& operator=(const ResourceScopeGuard&) = delete;

		ResourceScopeGuard(ResourceScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
			: m_func(std::move(other.m_func)), m_active(other.m_active)
		{
			other.m_active = false;
		}

		ResourceScopeGuard& operator=(ResourceScopeGuard&& other) noexcept(std::is_nothrow_move_assignable_v<F>)
		{
			if (this == &other)
				return *this;

			// 先结算自身原有的清理动作，再接管对方的
			Run();

			m_func = std::move(other.m_func);
			m_active = other.m_active;
			other.m_active = false;
			return *this;
		}

		/// @brief 放弃清理动作（正常路径下使用）
		void Dismiss() noexcept
		{
			m_active = false;
		}

		/// @brief 当前是否仍会执行清理动作
		[[nodiscard]] bool IsActive() const noexcept
		{
			return m_active;
		}
	private:
		void Run() noexcept
		{
			if (!m_active)
				return;

			m_active = false;

			try
			{
				m_func();
			}
			catch (...)
			{
				// 析构不得抛出
				std::terminate();
			}
		}

		F m_func;
		bool m_active = false;
	};

	template <typename F>
	ResourceScopeGuard(F&&) -> ResourceScopeGuard<std::decay_t<F>>;
}
