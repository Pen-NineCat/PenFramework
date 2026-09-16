// File /Native/Engine/DebugVerify/DebugVerify.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#pragma once
#include "../Core/Environment.h"
#include "../Exception/Exception.hpp"
#include <utility>
#ifdef PENFRAMEWORK_BUILD_DEBUG
#ifdef PENFRAMEWORK_OS_WIN32
#include <crtdbg.h>
#include <intrin.h>
#define DEBUG_REPORT_HANDLE(message) \
	if consteval \
	{ \
		throw message; \
	} \
	(void)((1 != _CrtDbgReportW(_CRT_ASSERT, __FILEW__, __LINE__, nullptr, L"%s", _CRT_WIDE(message))) || (_CrtDbgBreak(), 0)); \
	__fastfail(5); \
	std::unreachable();
#define DEBUG_REPORT_HANDLE_L(message) \
	if consteval \
	{ \
		throw message; \
	} \
	(void)((1 != _CrtDbgReportW(_CRT_ASSERT, __FILEW__, __LINE__, nullptr, L"%s", message)) || (_CrtDbgBreak(), 0)); \
	__fastfail(5); \
	std::unreachable();
#else // PENFRAMEWORK_OS_WIN32
#warning "PenFramework内部Debug检查器在当前SDK版本主要依靠MSVC crtdbg工具，在当前平台检查器以assert来替代。"
#include <cassert>
#define DEBUG_REPORT_HANDLE(message) \
	assert((message,false)); \
	std::unreachable();
//非Windows环境下不可能会用这个宏
#define DEBUG_REPORT_HANDLE_L(message) 
#error "非Windows环境下不可能会使用L版本的错误宏"
#endif // PENFRAMEWORK_OS_WIN32

// 如果条件不满足，中断程序运行，并且输出错误信息
#define DEBUG_VERIFY_REPORT(cond,message) \
	if(!(cond)) [[unlikely]] \
	{ \
		DEBUG_REPORT_HANDLE(message); \
	} 

// 立即中断程序运行，并且输出错误信息
#define DEBUG_ALWAYS_REPORT(message) \
	DEBUG_REPORT_HANDLE(message) \

#define DEBUG_VERIFY_REPORT_L(cond,message) \
	if(!(cond)) [[unlikely]] \
	{ \
		DEBUG_REPORT_HANDLE_L(message) \
	}

#define DEBUG_ALWAYS_REPORT_L(message) \
	DEBUG_REPORT_HANDLE_L(message) \

#else // _DEBUG
#define DEBUG_REPORT_HANDLE(message)
#define DEBUG_VERIFY_REPORT(cond,message)
#define DEBUG_ALWAYS_REPORT(message) std::unreachable();
#define DEBUG_VERIFY_REPORT_L(cond,message)
#define DEBUG_ALWAYS_REPORT_L(message) std::unreachable();
#endif // _DEBUG

#define DEBUG_VERIFY_REPORT_WITH_REL_OPERATION(cond,message,operation) \
	if (!(cond)) [[unlikely]] \
	{ \
		DEBUG_REPORT_HANDLE(message); \
		operation; \
	}

#define DEBUG_VERIFY_REPORT_WITH_REL_EXCEPTION(cond,message,exceptionType,...) \
	if(!(cond)) [[unlikely]] \
	{ \
		DEBUG_REPORT_HANDLE(message); \
		PenEngine::ThrowException(exceptionType(__VA_ARGS__)); \
	}

#define DEBUG_ALWAYS_REPORT_WITH_REL_OPERATION(message,operation) \
	DEBUG_REPORT_HANDLE(message); \
	operation;

#define DEBUG_ALWAYS_REPORT_WITH_REL_EXCEPTION(message,exceptionType,...) \
	DEBUG_REPORT_HANDLE(message); \
	PenEngine::ThrowException(exceptionType(__VA_ARGS__));