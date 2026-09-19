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

		/// @brief 请求取消该指令当前等待的外部操作，并让其尽快转为就绪
		/// @note 供调度器在 drain 超时等"必须结束"的路径上调用。
		///       默认实现返回 false，表示指令无法被主动取消（调度器会按其自身策略强杀）。
		///       实现者必须保证：Cancel() 返回 true 后，该指令最终会转为就绪，
		///       且不再持有任何会在自身销毁后继续写入其内存的在途操作（例如重叠IO需先取消并等待落地）。
		virtual bool Cancel() { return false; }
	};
}
