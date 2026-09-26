// File /Native/Engine/String/Format.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "String.hpp"
#include <format>

namespace PenEngine
{
	template <int = 0>
	String VFormat(std::string_view fmt, std::format_args args)
	{
		String str;

		#ifdef _MSC_VER
		// MSVC STL可以通过这个api计算出按照这个参数列表需要分配的估计大小，但是libcxx并没有找到类似的api
		str.Reserve(fmt.size() + args._Estimate_required_capacity());
		#endif // _MSC_VER

		std::vformat_to(std::back_insert_iterator(str), fmt, args);
		return str;
	}

	template <int = 0>
	void VFormatTo(String& buffer, std::string_view fmt, std::format_args args)
	{
		buffer.Clear();

		#ifdef _MSC_VER
		// MSVC STL可以通过这个api计算出按照这个参数列表需要分配的估计大小，但是libcxx并没有找到类似的api
		buffer.Reserve(fmt.size() + args._Estimate_required_capacity());
		#endif // _MSC_VER

		std::vformat_to(std::back_insert_iterator(buffer), fmt, args);
	}

	template <int = 0>
	WString VFormat(std::wstring_view fmt, std::wformat_args args)
	{
		WString str;

		#ifdef _MSC_VER
		// MSVC STL可以通过这个api计算出按照这个参数列表需要分配的估计大小，但是libcxx并没有找到类似的api
		str.Reserve(fmt.size() + args._Estimate_required_capacity());
		#endif // _MSC_VER

		std::vformat_to(std::back_insert_iterator(str), fmt, args);
		return str;
	}

	template <int = 0>
	void VFormatTo(WString& buffer, std::wstring_view fmt, std::wformat_args args)
	{
		buffer.Clear();
		#ifdef _MSC_VER
		// MSVC STL可以通过这个api计算出按照这个参数列表需要分配的估计大小，但是libcxx并没有找到类似的api
		buffer.Reserve(fmt.size() + args._Estimate_required_capacity());
		#endif // _MSC_VER

		std::vformat_to(std::back_insert_iterator(buffer), fmt, args);
	}

	template <typename... Args>
	String Format(std::format_string<Args...> fmt, Args&&... args)
	{
		return VFormat(fmt.get(), std::make_format_args(args...));
	}

	template <typename... Args>
	void FormatTo(String& buffer, std::format_string<Args...> fmt, Args&&... args)
	{
		return VFormatTo(buffer, fmt.get(), std::make_format_args(args...));
	}

	template <typename... Args>
	WString Format(std::wformat_string<Args...> fmt, Args&&... args)
	{
		return VFormatTo(fmt.get(), std::make_wformat_args(args...));
	}

	template <typename... Args>
	void FormatTo(WString& buffer, std::wformat_string<Args...> fmt, Args&&... args)
	{
		return VFormatTo(buffer, fmt.get(), std::make_wformat_args(args...));
	}
}
