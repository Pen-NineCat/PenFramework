// File /Native/Engine/IO/IOutputBuffer
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"

namespace PenEngine
{
	/// @brief 用于为输入输出设备提供用于读取数据的缓冲区
	class IOutputBuffer
	{
	public:
		virtual ~IOutputBuffer() noexcept = default;
		virtual Usize Size() const noexcept = 0;
		virtual const U8* Data() const noexcept = 0;
	};
}
