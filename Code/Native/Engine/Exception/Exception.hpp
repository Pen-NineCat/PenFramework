// File /Native/Engine/Exception/Exception.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"
#include <exception>
#include <source_location>
#include <stacktrace>
#include <string>

namespace PenEngine
{
	class Exception : public std::exception
	{
	public:
		Exception(std::string_view operation, std::string_view detail) :
			m_operation(operation),
			m_detail(detail)
		{}

		void SetCallPointData(std::stacktrace st, const std::source_location& sl)
		{
			m_stacktrace = std::move(st);
			m_sourceLocation = sl;
		}

		[[nodiscard]] std::string_view Operation() const noexcept
		{
			return m_operation;
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
	private:
		std::string m_operation;
		std::string m_detail;
		std::stacktrace m_stacktrace;
		std::source_location m_sourceLocation;
	};

	template <typename E> requires std::derived_from<E, Exception> || std::is_same_v<E, PenEngine::Exception>
	[[noreturn]] PENFRAMEWORK_FORCE_INLINE void ThrowException(E e, std::stacktrace st = std::stacktrace::current(), std::source_location sl = std::source_location::current())
	{
		e.SetCallPointData(st, sl);
		throw e;
	}

	template <typename E> requires ((std::derived_from<E, std::exception> || std::is_same_v<E, std::exception>) && !(std::derived_from<E, Exception> || std::is_same_v<E, PenEngine::Exception>))
		[[noreturn]] PENFRAMEWORK_FORCE_INLINE void ThrowException(E e)
	{
		throw e;
	}
}