// File /Native/Engine/String/StringUnorderedMap.hpp
// 
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "StringTransparentHash.hpp"
#include <unordered_map>

namespace PenEngine
{
	template <typename V>
	using StringUnorderedMap = std::unordered_map<String, V, StringTransparentHash<char>, std::equal_to<>>;
	template <typename V>
	using StringUnorderedMultimap = std::unordered_multimap<String, V, StringTransparentHash<char>, std::equal_to<>>;
}
