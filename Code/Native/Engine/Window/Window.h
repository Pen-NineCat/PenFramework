// File /Code/Native/Engine/Window/Window.h
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"
#include <concepts>



#ifdef PENFRAMEWORK_OS_WIN32
#include "Win32Window.h"
#endif

namespace PenEngine
{
	#ifdef PENFRAMEWORK_OS_WIN32
	using Window = Win32Window;
	#endif

	template <typename T>
	concept WindowTraits = requires(T window, const WindowInitContext & ctx)
	{
		{ window.Create(ctx) } -> std::same_as<WindowHandleData>;
	};

	static_assert(WindowTraits<Window>, "窗口类不符合要求");
}
