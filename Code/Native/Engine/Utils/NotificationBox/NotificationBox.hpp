// File /Native/Engine/Utils/NotificationBox/NotificationBox.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"

#include "Win32NotificationBox.h"

namespace PenEngine
{
	#ifdef PENFRAMEWORK_OS_WIN32
	using NotificationBox = Win32NotificationBox;
	#endif
}

