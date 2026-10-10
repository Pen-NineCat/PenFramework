// File /Code/Native/Engine/Exception/Exception.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"
#include <concepts> 
#include <exception>
#include <source_location>
#include <stacktrace>
#include <string>
#include <string_view>
#include <typeinfo>
#include <type_traits> 
#include <utility> 

namespace PenEngine
{
	class Exception : public std::exception
	{
	public:
		Exception(std::string_view exceptionType, std::string_view detail) :
			m_exceptionType(exceptionType),
			m_detail(detail)
		{}

		explicit Exception(const std::exception& e) :
			m_exceptionType(typeid(e).name()),
			m_detail(e.what())
		{}

		Exception(const Exception&) noexcept = default;
		Exception(Exception&&) noexcept = default;
		Exception& operator=(const Exception&) noexcept = default;
		Exception& operator=(Exception&&) noexcept = default;

		void SetCallPointData(std::stacktrace st, const std::source_location& sl)
		{
			m_stacktrace = std::move(st);
			m_sourceLocation = sl;
		}

		[[nodiscard]] std::string_view ExceptionType() const noexcept
		{
			return m_exceptionType;
		}

		[[nodiscard]] std::string_view Detail() const noexcept
		{
			return m_detail;
		}

		const std::stacktrace& Stacktrace() const noexcept
		{
			return m_stacktrace;
		}

		const std::source_location& SourceLocation() const noexcept
		{
			return m_sourceLocation;
		}

		virtual char const* what() const override
		{
			return m_detail.data();
		}
	private:
		std::string m_exceptionType;
		std::string m_detail;
		std::stacktrace m_stacktrace;
		std::source_location m_sourceLocation;
	};

	[[noreturn]] PENFRAMEWORK_FORCE_INLINE void ThrowException(Exception& e, std::stacktrace st = std::stacktrace::current(), std::source_location sl = std::source_location::current())
	{
		e.SetCallPointData(std::move(st), sl);
		throw e;
	}

	[[noreturn]] PENFRAMEWORK_FORCE_INLINE void ThrowException(const std::exception& e, std::stacktrace st = std::stacktrace::current(), std::source_location sl = std::source_location::current())
	{
		Exception pe(e);
		pe.SetCallPointData(std::move(st), sl);
		throw pe;
	}
}
