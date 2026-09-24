// File /Native/Engine/Core/CommandLineUtils.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "CommandLineUtils.h"

#ifdef PENFRAMEWORK_OS_WIN32
#include "../OS/Windows/Windows.h"
// WindowsMinDef.h 定义了 WIN32_LEAN_AND_MEAN，Windows.h 不再带入 shellapi.h；
// 而 CommandLineToArgvW 声明在这里，必须显式包含
#include <shellapi.h>

namespace PenEngine
{
	namespace
	{
		/// @brief LocalFree 的作用域守卫
		/// @note `CommandLineToArgvW` 的返回缓冲区由 `LocalAlloc` 分配，
		///       必须用 `LocalFree` 释放 —— 不能用 `free` / `delete`，也不能交给 PenMemory 的接管分配器
		class LocalAllocGuard
		{
		public:
			explicit LocalAllocGuard(HLOCAL handle) noexcept : m_handle(handle) {}
			~LocalAllocGuard() noexcept
			{
				if (m_handle != nullptr)
					LocalFree(m_handle);
			}

			LocalAllocGuard(const LocalAllocGuard&) = delete;
			LocalAllocGuard(LocalAllocGuard&&) = delete;
			LocalAllocGuard& operator=(const LocalAllocGuard&) = delete;
			LocalAllocGuard& operator=(LocalAllocGuard&&) = delete;

			[[nodiscard]] bool Valid() const noexcept { return m_handle != nullptr; }
		private:
			HLOCAL m_handle = nullptr;
		};

		/// @brief 把 `CommandLineToArgvW` 的结果转成 UTF-8 参数数组
		std::vector<String> ConvertAndFree(LPWSTR* rawArguments, int argumentCount)
		{
			std::vector<String> arguments;
			LocalAllocGuard guard(rawArguments);

			if (!guard.Valid() || rawArguments == nullptr || argumentCount <= 0)
				return arguments;

			arguments.reserve(static_cast<Usize>(argumentCount));
			for (int i = 0; i < argumentCount; ++i)
			{
				String argument;
				// 宽窄转换交给 BasicString 自身，非法 UTF-16 会经 ThrowException 抛 BadUTFConvertException
				argument.ConvertFrom(rawArguments[i]);
				arguments.push_back(std::move(argument));
			}

			return arguments;
		}
	}

	String CommandLineUtils::GetRawCommandLine()
	{
		String rawCommandLine;
		rawCommandLine.ConvertFrom(GetCommandLineW());
		return rawCommandLine;
	}

	std::vector<String> CommandLineUtils::GetArgumentVector()
	{
		// ⚠️ 必须分两步：`f(CommandLineToArgvW(..., &count), count)` 的实参求值顺序未指定，
		// MSVC 从右往左求值，会把尚未写入的 count（0）先读走，结果拿到空数组
		int argumentCount = 0;
		LPWSTR* rawArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
		return ConvertAndFree(rawArguments, argumentCount);
	}

	// I1：Windows 忽略传入的 argc/argv，改走宽字符 API —— 唯一的权威来源
	String CommandLineUtils::GetRawCommandLine(int, char*[])
	{
		return GetRawCommandLine();
	}

	std::vector<String> CommandLineUtils::GetArgumentVector(int, char*[])
	{
		return GetArgumentVector();
	}
}

#else // PENFRAMEWORK_OS_WIN32

namespace PenEngine
{
	// I1：非 Windows 直接用入口给的 argc/argv —— 平台已经按自己的规则拆分好了，
	// 这里只做编码归一到 UTF-8（BasicString 的宽窄转换）
	String CommandLineUtils::GetRawCommandLine(int argc, char* argv[])
	{
		String rawCommandLine;
		for (int i = 0; i < argc; ++i)
		{
			if (i > 0)
				rawCommandLine.PushBack(' ');
			rawCommandLine.ConvertAndPushBack(argv[i]);
		}
		return rawCommandLine;
	}

	std::vector<String> CommandLineUtils::GetArgumentVector(int argc, char* argv[])
	{
		std::vector<String> arguments;
		if (argc <= 0 || argv == nullptr)
			return arguments;

		arguments.reserve(static_cast<Usize>(argc));
		for (int i = 0; i < argc; ++i)
			arguments.emplace_back(argv[i]);

		return arguments;
	}
}

#endif // PENFRAMEWORK_OS_WIN32
