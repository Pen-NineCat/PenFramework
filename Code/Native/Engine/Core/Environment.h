// File /Native/Engine/Core/Environment.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#ifdef _WIN32
#define PENFRAMEWORK_OS_WIN32
#endif

#ifdef _WIN64
#define PENFRAMEWORK_OS_WIN64
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>

// iOS (Simulator or Device)
#if TARGET_OS_IPHONE
#define PENFRAMEWORK_OS_IOS
// macOS
#elif TARGET_OS_MAC
#define PENFRAMEWORK_OS_MACOS
#else
#error "Unknown Apple platform"
#endif
#endif

#if defined(__linux__) || defined(__linux)
#define PENFRAMEWORK_OS_LINUX
#endif

#ifdef __ANDROID__
#define PENFRAMEWORK_OS_ANDROID
#endif

#ifdef _MSC_VER
#define PENFRAMEWORK_COMPLIER_MSVC
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define PENFRAMEWORK_COMPLIER_GCC
#endif

#ifdef __clang__
#define PENFRAMEWORK_COMPLIER_CLANG
#endif

#if defined(_M_IX86) || defined(__i386__)
#define PENFRAMEWORK_ARCH_X86
#define PENFRAMEWORK_ARCH_32BIT
#elif defined(_M_X64) || defined(__x86_64__)
#define PENFRAMEWORK_ARCH_X64
#define PENFRAMEWORK_ARCH_64BIT
#endif

#if defined(_M_ARM) || defined(__arm__)
#define PENFRAMEWORK_ARCH_ARM
#define PENFRAMEWORK_ARCH_32BIT
#elif defined(_M_ARM64) || defined(__aarch64__)
#define PENFRAMEWORK_ARCH_ARM64
#define PENFRAMEWORK_ARCH_64BIT
#endif

#ifdef __riscv
#define PENFRAMEWORK_ARCH_RISCV
#endif

#if defined(PENFRAMEWORK_ARCH_X86) || defined(PENFRAMEWORK_ARCH_X64)
#define PENFRAMEWORK_INTRINSIC_SSE
#endif

#if defined(PENFRAMEWORK_ARCH_ARM) || defined(PENFRAMEWORK_ARCH_ARM64)
#define PENFRAMEWORK_INTRINSIC_NEON
#endif

#if defined(_DEBUG) || defined(DEBUG)
#define PENFRAMEWORK_BUILD_DEBUG
#endif

#if defined(NODEBUG) || defined(NDEBUG) 
#define PENFRAMEWORK_BUILD_RELEASE
#endif

#ifdef PENFRAMEWORK_COMPLIER_MSVC
#define PENFRAMEWORK_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#define PENFRAMEWORK_NO_VTABLE __declspec(novtable)
#define PENFRAMEWORK_FORCE_INLINE __forceinline
#else
#define PENFRAMEWORK_NO_UNIQUE_ADDRESS [[no_unique_address]]
#define PENFRAMEWORK_NO_VTABLE
#define PENFRAMEWORK_FORCE_INLINE __attribute __((always_inline))
#endif // PENFRAMEWORK_COMPLIER_MSVC

#define PENFRAMEWORK_SUPPORT_PINVOKE

#ifdef PENFRAMEWORK_OS_WIN32
#define PENFRAMEWORK_EXPORT_API __declspec(dllexport)
#define PENFRAMEWORK_IMPORT_API __declspec(dllimport)
#else
#define PENFRAMEWORK_EXPORT_API __attribute__((visibility("default")))
#define PENFRAMEWORK_IMPORT_API __attribute__((visibility("default")))
#endif //PENFRAMEWORK_OS_WIN32

#ifdef PENFRAMEWORK_SUPPORT_CONSTEXPR_EXCEPTION
#define PENFRAMEWORK_CONSTEXPR_EXCEPTION_FUNCTION  constexpr 
#else
#define PENFRAMEWORK_CONSTEXPR_EXCEPTION_FUNCTION 
#endif // PENFRAMEWORK_CONSTEXPR_EXCEPTION_FUNCTION 

#include <cstddef>
#include <cstdint>

namespace PenEngine
{
	using B8 = uint8_t;
	using U8 = uint8_t;
	using U16 = uint16_t;
	using U32 = uint32_t;
	using U64 = uint64_t;
	using I8 = int8_t;
	using I16 = int16_t;
	using I32 = int32_t;
	using I64 = int64_t;
	using Usize = size_t;
	using PtrDiff = ptrdiff_t;

	using HashID = U64;

	constexpr static Usize BitsPerBytes = 8;
}