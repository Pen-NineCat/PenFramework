// File /Native/Engine/String/StringTransparentHash.hpp
// 
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "String.hpp"

namespace PenEngine
{
	template <typename CharType>
	struct StringTransparentHash
	{
		using is_transparent = void;

		static Usize operator()(BasicStringView<CharType> str) noexcept
		{
			return std::hash<BasicStringView<CharType>>::operator()(str);
		}

		static Usize operator()(const BasicString<CharType>& str) noexcept
		{
			return std::hash<BasicString<CharType>>::operator()(str);
		}

		static Usize operator()(const CharType* ptr) noexcept
		{
			return std::hash<BasicStringView<CharType>>::operator()(BasicStringView<CharType>(ptr));
		}
	};
}