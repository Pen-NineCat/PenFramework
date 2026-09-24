// File /Native/Engine/Core/CommandLineUtils.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../String/String.hpp"
#include <vector>

namespace PenEngine
{
	/// @brief 进程启动参数的权威来源
	/// @note I1. 取参形态**按平台分派**，调用方一律传入口的 `argc/argv`：
	///       - Windows：忽略 `argc/argv`，改走 `GetCommandLineW` + `CommandLineToArgvW`。
	///         原因是入口形态本身不可靠：`main` 的 `argv` 经过 CRT 的窄字符转换，
	///         非 ASCII 会丢；Native 构建下入口是 `wWinMain`，它根本没有 `argv`。
	///         统一走宽字符 API 才能让 Terminal 与 Native 两种构建解析出同一结果。
	///       - 其他平台：直接用入口给的 `argc/argv`（已按平台规则拆分好，无需再解析）。
	///       I2. 因此「传了 argc/argv 也可能被忽略」是**有意行为**，不是遗漏。
	///       I3. 重定向与管道下 `GetCommandLineW` 同样有效；
	///       返回值一次性拷贝为 UTF-8 `String`，不保留任何指向系统缓冲区的引用。
	class CommandLineUtils
	{
	public:
		CommandLineUtils() = delete;

		/// @brief 取完整原始命令行（含 exe 路径）
		/// @return UTF-8 编码的完整命令行；取不到时返回空串
		/// @note Windows 返回 `GetCommandLineW` 的原文；其他平台把 `argv` 重新拼回一行
		static String GetRawCommandLine(int argc, char* argv[]);

		/// @brief 取按平台规则拆分后的参数数组（见 I1 的平台分派）
		/// @return 第 0 项为 exe 路径（或平台等价物），其后为参数；取不到时返回空 vector
		/// @note Windows 走 `CommandLineToArgvW` + `LocalFree`。
		///       ⚠️ 该 API 的返回缓冲区由 `LocalAlloc` 分配，**不能**用 `free`/`delete` 释放，
		///       也不能交给 PenMemory —— 本函数在内部完成 UTF-16→UTF-8 转换后立刻 `LocalFree`，
		///       调用方拿到的只有 UTF-8 副本。
		/// @note 命令行中若含非法 UTF-16 序列，转换会抛 `PenEngine::Exception`（经 `ThrowException`）。
		static std::vector<String> GetArgumentVector(int argc, char* argv[]);

#ifdef PENFRAMEWORK_OS_WIN32
		/// @brief 不依赖入口参数的取参：仅供没有 `argc/argv` 的调用点（如 `wWinMain` 之后的路径）
		/// @note 只在 Windows 提供 —— 其他平台没有"脱离入口取参"这一说，调用方本就有 `argc/argv`
		static String GetRawCommandLine();
		static std::vector<String> GetArgumentVector();
#endif // PENFRAMEWORK_OS_WIN32
	};
}
