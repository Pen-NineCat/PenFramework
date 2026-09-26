// File /Native/Engine/Utils/OwnerThread.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../DebugTools/DebugVerify.hpp"
#include "../Exception/Exception.hpp"
#include <string>
#include <string_view>
#include <thread>

namespace PenEngine
{
	/// @brief 核心插件（单例 / 主线程独占组件）被非归属线程访问
	///
	/// 归属线程是这些组件的硬约束：`CoroutineScheduler`、`DeferredDestroyQueue`、
	/// `NotificationBox` 及平台后端都只允许在首次构造它们的线程（即主线程）上访问。
	/// 违反时统一抛本异常：Debug 下先由 `DEBUG_VERIFY_REPORT` 中断（`__fastfail`），
	/// Release 下抛出，`ExceptionType()` 恒为 `"CorePluginThreadViolation"`。
	class CorePluginThreadViolation : public Exception
	{
	public:
		/// @param Component 组件名（如 "CoroutineScheduler"）
		/// @param Detail 该组件自己的说明（哪个接口、为什么必须同线程）
		CorePluginThreadViolation(std::string_view Component, std::string_view Detail)
			: Exception("CorePluginThreadViolation", std::string(Component) + "：" + std::string(Detail))
		{}
	};

	/// @brief 对象级"归属线程"守卫：记录创建（或显式绑定）它的线程，并提供同线程校验
	///
	/// 用途：引擎里大量组件（尤其单例）只允许被**创建它的那个线程**访问 —— 单例总是首次在主线程
	/// `GetInstance()` 时构造，因此"构造线程"天然就是主线程。此前每个组件各自持有
	/// `std::thread::id m_ownerThread` 并各写一遍判定，本类型把这套机制收敛到一处。
	///
	/// 用法（成员组合，公开 API 保持不变）：
	/// ```
	/// 	class SomeComponent
	/// 	{
	/// 	public:
	/// 		[[nodiscard]] bool IsOwnerThread() const noexcept { return m_ownerThread.IsOwner(); }
	/// 	private:
	/// 		OwnerThread m_ownerThread;   // 构造即绑定当前线程
	/// 	};
	/// ```
	///
	/// 校验一律**同一强度**：Debug 报告并中断，Release 抛异常。两个宏只差异常类型：
	/// - `PENFRAMEWORK_VERIFY_CORE_PLUGIN_OWNER_THREAD` —— 核心插件用，异常固定为
	///   `CorePluginThreadViolation`
	/// - `PENFRAMEWORK_VERIFY_OWNER_THREAD_OR_THROW`   —— 通用形式，异常类型由调用方给
	///   （非核心插件但同样有归属线程约束的类型，如 `Thread`，用 `BadThreadOperation`）
	///
	/// @note 本类型不持有任何共享状态，只是 `std::thread::id` 的薄封装；
	///       跨线程读取它本身是无竞争的（`std::thread::id` 拷贝即可，且绑定只在初始化期发生）
	/// @note 若某个组件在 `Initialize()` 阶段才确定归属线程（如 `NotificationBox`），
	///       用 `Bind()` 重绑即可，不必另设字段
	class OwnerThread
	{
	public:
		/// @brief 构造即把当前线程记为归属线程
		OwnerThread() noexcept = default;

		/// @brief 重新把**当前**线程绑定为归属线程
		/// @note 只应在单线程初始化阶段调用（例如组件的 `Initialize()`）
		void Bind() noexcept
		{
			m_owner = std::this_thread::get_id();
		}

		/// @brief 绑定指定的归属线程
		void Bind(std::thread::id owner) noexcept
		{
			m_owner = owner;
		}

		/// @brief 归属线程的 ID
		[[nodiscard]] std::thread::id Owner() const noexcept
		{
			return m_owner;
		}

		/// @brief 当前线程是否为归属线程
		[[nodiscard]] bool IsOwner() const noexcept
		{
			return std::this_thread::get_id() == m_owner;
		}
	private:
		std::thread::id m_owner = std::this_thread::get_id();
	};

	/// @brief 通用的归属线程校验：Debug 报告并中断，Release 抛出给定异常
	/// @param OwnerThreadGuard `OwnerThread` 类型的一个**表达式**（对象或返回引用的调用）
	/// @param Message 必须是**字面量**：`DEBUG_REPORT_HANDLE` 用 `_CRT_WIDE`（即 `L##x`）包装它，
	///        传变量只会把变量名本身印出来
	/// @param ExceptionType 异常类型（需派生自 `PenEngine::Exception`），其余参数透传给其构造函数
	#define PENFRAMEWORK_VERIFY_OWNER_THREAD_OR_THROW(OwnerThreadGuard, Message, ExceptionType, ...) \
		DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION((OwnerThreadGuard).IsOwner(), Message, ExceptionType, __VA_ARGS__)

	/// @brief 核心插件的归属线程校验：Debug 报告并中断，Release 抛 `CorePluginThreadViolation`
	/// @param Component 组件名（字面量）
	/// @param Detail 该组件自己的说明（字面量或表达式）
	/// @note `Message` 单独给一份是必须的：Debug 的报告宏要求字面量，而异常详情允许是表达式
	#define PENFRAMEWORK_VERIFY_CORE_PLUGIN_OWNER_THREAD(OwnerThreadGuard, Message, Component, Detail) \
		PENFRAMEWORK_VERIFY_OWNER_THREAD_OR_THROW(OwnerThreadGuard, Message, CorePluginThreadViolation, Component, Detail)
}
