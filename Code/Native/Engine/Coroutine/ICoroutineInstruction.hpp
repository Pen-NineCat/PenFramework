// File /Native/Engine/Coroutine/ICoroutineInstruction.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <coroutine>
#include "../Core/Environment.h"

namespace PenEngine
{
	struct ICoroutineInstruction
	{
		virtual ~ICoroutineInstruction() = default;
		[[nodiscard]] virtual bool IsReady() = 0;

		/// @brief 指令的等待器
		/// await_suspend时把指令自身注册为父协程的当前等待对象，随后挂起父协程，等待调度器轮询IsReady后恢复
		struct Awaitable
		{
			[[nodiscard]] bool await_ready() const noexcept
			{
				return m_instruction->IsReady();
			}

			template <typename PromiseType>
			std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> coroutine) const noexcept
			{
				coroutine.promise().SetAwaitedObject(m_instruction);
				return std::noop_coroutine();
			}

			static void await_resume() noexcept {}

			ICoroutineInstruction* m_instruction = nullptr;
		};

		Awaitable operator co_await() noexcept
		{
			return Awaitable{ this };
		}
	};
}
