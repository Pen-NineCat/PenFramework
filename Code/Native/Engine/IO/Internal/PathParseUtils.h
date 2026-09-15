// File /Native/Engine/IO/Internal/PathParseUtils.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#pragma once

#include "../../Core/Environment.h"

namespace PenEngine::Internal
{
	const char* ForwardTrySkipRootName(const char* start, const char* end) noexcept;
	const char* ForwardTrySkipDriveRoot(const char* start, const char* end) noexcept;
	const char* ForwardTrySkipNetworkRoot(const char* start, const char* end) noexcept;
		
	// @brief 尝试跳过start到end区间的名称
	// @retval nullptr start为空，或者==end，或者以'/'开始
	// @retval ptr 返回区间内的下一个'/'所在位置指针
	const char* ForwardTrySkipName(const char* start, const char* end) noexcept;
	const char* ForwardTrySkipAllSeparator(const char* start, const char* end) noexcept;
	const char* ForwardTrySkipCountedSeparator(const char* start, const char* end, Usize count) noexcept;

	const char* BackwardTrySkipRootName(const char* start, const char* end) noexcept;
	const char* BackwardTrySkipDriveRoot(const char* start, const char* end) noexcept;
	const char* BackwardTrySkipNetworkRoot(const char* start, const char* end) noexcept;

	// @brief 尝试跳过start到end区间的名称
	// @retval nullptr start为空，或者==end，或者以'/'开始
	// @retval ptr 返回区间内的下一个'/'所在位置指针
	const char* BackwardTrySkipName(const char* start, const char* end) noexcept;
	const char* BackwardTrySkipAllSeparator(const char* start, const char* end) noexcept;
	const char* BackwardTrySkipCountedSeparator(const char* start, const char* end, Usize count) noexcept;

	bool IsValidSeparator(char ch) noexcept;
	bool IsValidDriveLetter(char ch) noexcept;
}