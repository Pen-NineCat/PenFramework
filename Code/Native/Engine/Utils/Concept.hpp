// File /Native/Engine/Utils/Concept.hpp
// 
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <type_traits>

namespace PenEngine
{
	template <typename T, typename... Ts>
	concept IsOneOf = (std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<Ts>> || ...);

	template <typename From, typename To>
	concept IsStaticCastable = requires(From && t) { static_cast<To>(std::forward<From>(t)); };

	template <typename T, template<typename> class Template>
	struct IsTemplateInstanceOfSt : std::false_type {};

	template <typename T, template<typename> class Template>
	struct IsTemplateInstanceOfSt<Template<T>,Template> : std::true_type{};

	template <typename T, template<typename> class Template>
	concept IsTemplateInstanceOf = IsTemplateInstanceOfSt<T,Template>::value;
}
