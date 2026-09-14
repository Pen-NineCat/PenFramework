// File /Native/Engine/Core/Window/WindowContext
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Environment.h"
#include "../../String/String.hpp"

#ifdef PENFRAMEWORK_OS_WIN32
#include "../../OS/Windows/Windows.h"
#endif // PENFRAMEWORK_OS_WIN32

namespace PenEngine
{
	class CoreApplication;

	struct WindowInitContext
	{
		U32 Width;
		U32 Height;

		bool Maximize;

		String ApplicationTitle;

#ifdef PENFRAMEWORK_OS_WIN32
		HINSTANCE HInstance;
#endif // PENFRAMEWORK_OS_WIN32
		CoreApplication* CoreApplication;
	};
}
