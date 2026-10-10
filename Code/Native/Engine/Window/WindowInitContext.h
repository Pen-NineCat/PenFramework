// File /Code/Native/Engine/Window/WindowInitContext.h
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"

#include "../String/String.hpp"
#ifdef PENFRAMEWORK_OS_WIN32
#include "../OS/Win/Windows.h"
#endif

namespace PenEngine
{
	struct WindowInitContext
	{
		StringView Title;

		U16 Width;
		U16 Height;
		
		#ifdef PENFRAMEWORK_OS_WIN32
		HINSTANCE HInstance;
		#endif // PENFRAMEWORK_OS_WIN32

		bool Maximized;


	};
}
