// File /Native/Engine/IO/FileDevice.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"

#ifdef PENFRAMEWORK_OS_WIN32
#include "Win32FileDevice.h"
#endif

namespace PenEngine
{
	#ifdef PENFRAMEWORK_OS_WIN32
	using FileDevice = Win32FileDevice;
	#endif
}
