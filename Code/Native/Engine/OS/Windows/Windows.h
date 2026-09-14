// File /Engine/OSPlatform/Windows/Windows.h
// This file is a part of PenFramework Project
// https://github.com/Pen-NineCat/PenFramework
// 
// Copyright (C) 2025 - Present PenNineCat. All rights reserved
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

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