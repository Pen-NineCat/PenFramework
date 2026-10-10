// File /Code/Native/Engine/Window/WindowHandleData.h
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"

namespace PenEngine
{
	struct WindowHandleData
	{
#ifdef PENFRAMEWORK_OS_WIN32
		void* Hwnd;
#endif
	};
}
