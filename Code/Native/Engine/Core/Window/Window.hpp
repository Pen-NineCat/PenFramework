// File /Native/Engine/Core/Window/Window.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Environment.h"

#ifdef PENFRAMEWORK_OS_WIN32
#include "Win32Window.h"
#endif // PENFRAMEWORK_OS_WIN32

namespace PenEngine
{
	using OSWindow = Win32Window;
}
