// File /Native/Memory/tests/MiniTest.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// A three-macro test harness: no external dependencies, no allocation from the
// allocator under test unless the test explicitly asks for it.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

#include "../Common.hpp"
#include "../Interface.h"

namespace PenMemoryTest
{

	inline int gFailures = 0;
	inline int gChecks = 0;
	inline const char* gCurrentTest = "<none>";

	inline void Fail(const char* file, int line, const std::string& message)
	{
		std::fprintf(stderr, "  FAIL %s at %s:%d\n       %s\n", gCurrentTest, file,
		   line, message.c_str());
		gFailures++;
	}

	inline void Expect(bool ok)
	{
		gChecks++;
		(void)ok;
	}

	inline int Summary(const char* suite)
	{
		if (gFailures == 0)
		{
			std::printf("[ ok ] %-22s %d checks passed\n", suite, gChecks);
			return 0;
		}
		std::printf("[FAIL] %-22s %d of %d checks failed\n", suite, gFailures,
					gChecks);
		return 1;
	}

	// Renders a value for the failure message.  Deliberately simple: only the types
	// the tests compare are supported.
	template <typename T>
	inline std::string ToString(T value)
	{
		if constexpr (std::is_same_v<T, PenMemory::Length>)
		{
			return std::to_string(value.RawNum()) + " pages";
		}
		else if constexpr (std::is_same_v<T, PenMemory::PageId>)
		{
			return std::to_string(value.Index()) + " page";
		}
		else if constexpr (std::is_pointer_v<T>)
		{
			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%p",
			  static_cast<const void*>(value));
			return buffer;
		}
		else if constexpr (std::is_enum_v<T>)
		{
			return std::to_string(static_cast<long long>(value));
		}
		else if constexpr (std::is_arithmetic_v<T>)
		{
			return std::to_string(value);
		}
		else
		{
			return "?";
		}
	}

}  // namespace PenMemoryTest

#define TEST(name) PenMemoryTest::gCurrentTest = name

#define EXPECT(cond)                                                          \
  do {                                                                        \
    PenMemoryTest::Expect(!!(cond));                                          \
    if (!(cond)) PenMemoryTest::Fail(__FILE__, __LINE__, "expected: " #cond); \
  } while (0)

#define EXPECT_MSG(cond, msg)                                                \
  do {                                                                       \
    PenMemoryTest::Expect(!!(cond));                                         \
    if (!(cond))                                                             \
      PenMemoryTest::Fail(__FILE__, __LINE__,                                \
                          std::string("expected: " #cond " (") + (msg) + ")"); \
  } while (0)

#define EXPECT_EQ(a, b)                                                        \
  do {                                                                         \
    const auto va_ = (a);                                                      \
    const auto vb_ = (b);                                                      \
    PenMemoryTest::Expect(va_ == vb_);                                         \
    if (!(va_ == vb_)) {                                                       \
      PenMemoryTest::Fail(__FILE__, __LINE__,                                  \
                          std::string("expected " #a " == " #b ", got ") +     \
                              PenMemoryTest::ToString(va_) + " vs " +          \
                              PenMemoryTest::ToString(vb_));                   \
    }                                                                          \
  } while (0)

#define EXPECT_GE(a, b)                                                        \
  do {                                                                         \
    const auto va_ = (a);                                                      \
    const auto vb_ = (b);                                                      \
    PenMemoryTest::Expect(va_ >= vb_);                                         \
    if (!(va_ >= vb_)) {                                                       \
      PenMemoryTest::Fail(__FILE__, __LINE__,                                  \
                          std::string("expected " #a " >= " #b ", got ") +     \
                              PenMemoryTest::ToString(va_) + " vs " +          \
                              PenMemoryTest::ToString(vb_));                   \
    }                                                                          \
  } while (0)

#define EXPECT_LT(a, b)                                                        \
  do {                                                                         \
    const auto va_ = (a);                                                      \
    const auto vb_ = (b);                                                      \
    PenMemoryTest::Expect(va_ < vb_);                                          \
    if (!(va_ < vb_)) {                                                        \
      PenMemoryTest::Fail(__FILE__, __LINE__,                                  \
                          std::string("expected " #a " < " #b ", got ") +      \
                              PenMemoryTest::ToString(va_) + " vs " +          \
                              PenMemoryTest::ToString(vb_));                   \
    }                                                                          \
  } while (0)

#define EXPECT_LE(a, b)                                                        \
  do {                                                                         \
    const auto va_ = (a);                                                      \
    const auto vb_ = (b);                                                      \
    PenMemoryTest::Expect(va_ <= vb_);                                         \
    if (!(va_ <= vb_)) {                                                       \
      PenMemoryTest::Fail(__FILE__, __LINE__,                                  \
                          std::string("expected " #a " <= " #b ", got ") +     \
                              PenMemoryTest::ToString(va_) + " vs " +          \
                              PenMemoryTest::ToString(vb_));                   \
    }                                                                          \
  } while (0)

// Verifies every invariant of every layer.  Called after each workload.
#define CHECK_HEAP()                                                          \
  do {                                                                        \
    const char* error_ = nullptr;                                             \
    PenMemoryTest::Expect(true);                                              \
    if (!::VerifyHeap(&error_)) {                                             \
      PenMemoryTest::Fail(                                                    \
          __FILE__, __LINE__,                                                 \
          std::string("VerifyHeap failed: ") + (error_ ? error_ : ""));       \
    }                                                                         \
  } while (0)
