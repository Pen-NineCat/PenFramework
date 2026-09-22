// File /Native/Engine/Object/Internal/MetaFunction.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../../String/StringView.hpp"

namespace PenEngine::Internal
{
	constexpr HashID CalculateClassMetaHash(StringView str) noexcept
	{
		constexpr HashID fnvOffsetBasis = 14695981039346656037ULL;
		constexpr HashID fnvPrime = 1099511628211ULL;

		HashID hash = fnvOffsetBasis;

		// constexpr 循环计算
		for (HashID i = 0; i < str.Size(); ++i) {
			hash ^= static_cast<Usize>(str[i]);
			hash *= fnvPrime;
		}

		return hash;
	}
}
