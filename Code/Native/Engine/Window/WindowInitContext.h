// File /Code/Native/Engine/Window/WindowInitContext.h
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"

#include "../String/String.hpp"

namespace PenEngine
{
	struct WindowInitContext
	{
		StringView Title;

		U16 Width;
		U16 Height;
		
		bool IsMaximized;
	};
}
