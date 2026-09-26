// File /Native/Engine/Thread/ThreadTypes.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include <thread>

namespace PenEngine
{
	/// @brief 原生线程 ID（操作系统域，不是 std::thread::id）
	/// @note Win32：与 `GetCurrentThreadId()` / `GetThreadId(handle)` 同域，`U32`，线程 ID 永不为 0；
	///       POSIX：直接是 `pthread_t`（按值比较），以 `U64` 承载
	/// @note 线程结束后该 ID 可能被系统复用，因此只适合标识 / 日志 / 排障，不保证唯一性
#ifdef PENFRAMEWORK_OS_WIN32
	using NativeThreadID = U32;
#else
	using NativeThreadID = U64;
#endif

	/// @brief 逻辑处理器编号（跨处理器组的全局编号）
	/// @note 编码为 `处理器组号 * 64 + 组内位号`，因此单组机器上就等于
	///       `GetCurrentProcessorNumber()`；多组机器上旧写法 `1ull << 组内序号` 会错绑，
	///       本模块统一用这个全局编号 + `SetThreadGroupAffinity` 处理
	using CoreID = U32;

	/// @brief 原生线程句柄
	/// @note Win32：`HANDLE`（`void*`）；POSIX：`pthread_t`。由 STL 给出平台正确的类型
	using NativeThreadHandle = std::jthread::native_handle_type;

	/// @brief 核心绑定策略
	/// @note 亲和性是"每个线程一份"的状态：成员版 `Thread::LockThreadCore` 作用于该对象的 worker，
	///       静态版 `Thread::LockThisThreadCore` 作用于调用者自身
	enum class LockThreadCorePolicy : U8
	{
		OnCurrentCore,          ///< 绑到调用者当前所在的那一个逻辑处理器
		OnHighEfficiencyCore,   ///< 绑到一个高性能核（P-core）；机器只有一类核时等价于"任选一核"
		OnLowEfficiencyCore,    ///< 绑到一个能效核（E-core）；机器没有更低能效的核时**返回 false**
		OnAnyCore,              ///< 解除绑定：恢复进程默认亲和性（全部可用核心）
	};
}
