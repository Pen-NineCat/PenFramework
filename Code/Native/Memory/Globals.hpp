// File /Native/Memory/Globals.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: the process-wide allocator state.
//
// `Globals` is the equivalent of tcmalloc's `tc_globals`: the process-wide
// singletons (size class map, page map, page cache, central cache) that the
// layers below are wired to.
//
// This is internal C++ state and is deliberately kept out of the exported C ABI
// (Interface.h): only the implementation and the tests include this header.

#pragma once

#include <type_traits>

#include "CentralCache.h"
#include "Common.hpp"
#include "PageCache.h"
#include "Radix.h"
#include "ThreadCache.h"

namespace PenMemory
{
	class Globals
	{
	public:
		// Constructed on first use; thread safe.  Trivially destructible, so there
		// is no static destruction order problem with thread caches that outlive
		// main().
		static Globals& Get();

		Globals(const Globals&) = delete;
		Globals& operator=(const Globals&) = delete;

		SizeMap SizeClasses;
		PageMap PageMaps;
		PageCache Pages;
		CentralCache Central;

	private:
		Globals();
	};

	static_assert(std::is_trivially_destructible_v<Globals>,
		  "Globals must not need destruction");
}
