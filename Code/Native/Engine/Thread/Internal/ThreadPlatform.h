// File /Native/Engine/Thread/Internal/ThreadPlatform.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../ThreadTypes.hpp"

namespace PenEngine::Internal
{
	/// 本文件是 Thread 模块的平台原语层：只放"没有任何上层状态"的线程/处理器查询与设置，
	/// 供 Thread 以及将来的线程池、Mutex 等复用。
	///
	/// 纪律：
	/// - 平台头（Windows.h / pthread.h）只允许出现在本模块的 .cpp 里，不进公开头；
	/// - 所有函数都不抛出，失败一律用返回值表达（查询类返回 0/空，设置类返回 false）；
	/// - 除 `Topology` 的轮转游标外，内部缓存全程只读。

	/// @brief 当前线程的原生 ID
	[[nodiscard]] NativeThreadID ThisThreadID() noexcept;

	/// @brief 当前线程的原生句柄
	/// @note Win32 返回 `GetCurrentThread()` 伪句柄（无需关闭）；POSIX 返回 `pthread_self()`
	[[nodiscard]] NativeThreadHandle ThisThreadNativeHandle() noexcept;

	/// @brief 当前线程此刻所在的逻辑处理器编号
	/// @note Win32 用 `GetCurrentProcessorNumberEx`（跨处理器组正确）；POSIX 用 `sched_getcpu()`
	/// @note 该值会随调度迁移，只表示"问这一下的瞬间"
	[[nodiscard]] CoreID ThisCoreIndex() noexcept;

	/// @brief 逻辑处理器总数（跨处理器组的实际数目）
	/// @note 首次调用时枚举 `RelationProcessorCore` 并缓存：MSVC 的
	///       `std::thread::hardware_concurrency()` 走的是只报当前组的路径，
	///       `GetSystemInfo` 同理，故不用它们作为主路径（仅作查询失败时的兜底）
	[[nodiscard]] U32 HardwareConcurrency() noexcept;

	/// @brief 由线程句柄取原生线程 ID
	/// @note Win32：`GetThreadId(handle)`；POSIX：句柄本身就是 ID
	/// @return 句柄为空或查询失败时返回 0（不是合法线程 ID）
	[[nodiscard]] NativeThreadID ThreadIDOf(NativeThreadHandle handle) noexcept;

	/// @brief 把线程绑到指定逻辑处理器
	/// @param handle 目标线程句柄；为空表示"调用者自身"
	/// @param index 逻辑处理器编号（编码见 `CoreID`）
	/// @return 是否成功（越界、策略不允许、句柄失效等都会失败，不静默假装成功）
	bool LockThreadCoreToIndex(NativeThreadHandle handle, CoreID index) noexcept;

	/// @brief 解除绑定，恢复进程默认亲和性
	/// @param handle 目标线程句柄；为空表示"调用者自身"
	/// @note Win32 用进程亲和性掩码恢复；该 API 只作用于调用线程所在的处理器组，
	///       因此 >64 逻辑处理器（多组）的机器上只恢复当前组
	/// @note POSIX 侧没有"进程默认掩码"可查，按在线处理器数铺满
	bool UnlockThreadCore(NativeThreadHandle handle) noexcept;

	/// @brief 按策略把线程绑到某个核心
	/// @param handle 目标线程句柄；为空表示"调用者自身"
	/// @note `OnHighEfficiencyCore` 在"只有一类核"的机器上等价于任选一核（成功）；
	///       `OnLowEfficiencyCore` 在同样情形下返回 false（不存在更低能效的核，明确失败）
	bool LockThreadCore(NativeThreadHandle handle, LockThreadCorePolicy policy) noexcept;
}
