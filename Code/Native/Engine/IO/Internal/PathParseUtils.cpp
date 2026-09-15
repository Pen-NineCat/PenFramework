// File /Native/Engine/IO/Internal/PathParseUtils.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#include "PathParseUtils.h"

namespace PenEngine::Internal
{
	const char* ForwardTrySkipRootName(const char* start, const char* end) noexcept
	{
		#ifdef PENFRAMEWORK_OS_WIN32
		// Windows环境

		// 先检查是不是本地驱动器标识路径
		if (auto ret = ForwardTrySkipDriveRoot(start, end); ret != nullptr)
			return ret;
		// 再检查是不是网络驱动器标识路径
		if (auto ret = ForwardTrySkipNetworkRoot(start, end); ret != nullptr)
			return ret;

		#endif // PENFRAMEWORK_OS_WIN32

		// Unix Like环境下，RootName是不存在的
		return nullptr;
	}

	const char* ForwardTrySkipDriveRoot(const char* start, const char* end) noexcept
	{
		if (start == nullptr || start == end)
			return nullptr;

		// 示例情况
		// start -> "1:\.."
		// start[0] != 'a'~'z' || 'A'~'Z' 
		// start[1] == ':'
		// ret nullptr

		// start -> "C\..."
		// start[0] == 'a'~'z' || 'A'~'Z' 
		// start[1] != ':'
		// ret nullptr

		// start -> "C:"
		// end == ":"
		// start + 1 == end
		// ret nullptr

		if (start + 1 == end || !IsValidDriveLetter(start[0]) || start[1] != ':')
			return nullptr;

		return start + 2;
	}

	const char* ForwardTrySkipNetworkRoot(const char* start, const char* end) noexcept
	{
		if (start == nullptr || start == end)
			return nullptr;

		return ForwardTrySkipName(ForwardTrySkipCountedSeparator(start, end, 2), end);
	}

	const char* ForwardTrySkipName(const char* start, const char* end) noexcept
	{
		if (start == nullptr || start == end || IsValidSeparator(*start))
			return nullptr;

		start += 1;

		while (start != end && !IsValidSeparator(*start))
			start += 1;

		return start;
	}

	const char* ForwardTrySkipAllSeparator(const char* start, const char* end) noexcept
	{
		if (start == nullptr || start == end)
			return nullptr;

		if (!IsValidSeparator(*start))
			return nullptr;

		start += 1;

		while (start != end && IsValidSeparator(*start))
			start += 1;

		return start;
	}

	const char* ForwardTrySkipCountedSeparator(const char* start, const char* end, Usize count) noexcept
	{
		const char* ret = ForwardTrySkipAllSeparator(start, end);

		if (ret == nullptr)
			return nullptr;

		if (ret == start + count)
			return ret;

		return nullptr;
	}

	const char* BackwardTrySkipRootName(const char* start, const char* end) noexcept
	{
		#ifdef PENFRAMEWORK_OS_WIN32
		if (const char* ret = BackwardTrySkipDriveRoot(start, end))
			return ret;
		if (const char* ret = BackwardTrySkipNetworkRoot(start, end))
			return ret;
		#endif // PENFRAMEWORK_OS_WIN32

		return nullptr;
	}

	const char* BackwardTrySkipDriveRoot(const char* start, const char* end) noexcept
	{
		if (start == end)
			return nullptr;

		if (start - 1 == end || !IsValidDriveLetter(*(start - 1)) || start[1] != ':')
			return nullptr;

		return start - 2;
	}

	const char* BackwardTrySkipNetworkRoot(const char* start, const char* end) noexcept
	{
		if (start == end)
			return nullptr;

		return BackwardTrySkipCountedSeparator(BackwardTrySkipName(start, end), end, 2);
	}

	const char* BackwardTrySkipName(const char* start, const char* end) noexcept
	{
		if (start == nullptr || start == end || IsValidSeparator(*start))
			return nullptr;

		start -= 1;

		while (start != end && !IsValidSeparator(*start))
			start -= 1;

		if (start == end)
		{
			const char* ret = BackwardTrySkipRootName(start, end + 1);
			if (ret != nullptr)
				return ret - 1;
		}

		return start;
	}

	const char* BackwardTrySkipAllSeparator(const char* start, const char* end) noexcept
	{
		if (start == nullptr || start == end)
			return nullptr;

		if (!IsValidSeparator(*start))
			return nullptr;

		start -= 1;

		while (start != end && IsValidSeparator(*start))
			start -= 1;

		return start;
	}

	const char* BackwardTrySkipCountedSeparator(const char* start, const char* end, Usize count) noexcept
	{
		const char* ret = BackwardTrySkipAllSeparator(start, end);

		if (ret == nullptr)
			return nullptr;

		if (ret == start - count)
			return ret;

		return nullptr;
	}

	bool IsValidSeparator(char ch) noexcept
	{
		if (ch == '/')
			return true;
		#ifdef PENFRAMEWORK_OS_WIN32
		if (ch == '\\')
			return true;
		#endif // PENFRAMEWORK_OS_WIN32
		return false;
	}

	bool IsValidDriveLetter(char ch) noexcept
	{
		return (ch <= 'z' && ch >= 'a') || (ch <= 'Z' && ch >= 'A');
	}
}