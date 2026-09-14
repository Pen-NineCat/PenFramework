// File /Native/Engine/Exception/Exception.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include <exception>
#include <source_location>
#include <string>
#include <stacktrace>

namespace PenEngine
{
	class Exception : public std::exception
	{
	public:
		Exception(std::string_view operation, std::string_view detail, std::stacktrace stacktrace = std::stacktrace::current(), const std::source_location& sourceLocation = std::source_location::current()) :
			m_operation(operation),
			m_detail(detail),
			m_stacktrace(std::move(stacktrace)),
			m_sourceLocation(sourceLocation)
		{}
	private:
		std::string m_operation;
		std::string m_detail;
		std::stacktrace m_stacktrace;
		std::source_location m_sourceLocation;
	};
}
