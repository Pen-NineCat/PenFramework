// File /Native/Engine/OSPlatform/Windows/Windows.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "WindowsMinDef.h"
#include <Windows.h> // 基本Windows API
#include <windowsx.h> // 扩展Windows API
#include <wrl/client.h> // ComPtr

namespace PenEngine
{
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
}