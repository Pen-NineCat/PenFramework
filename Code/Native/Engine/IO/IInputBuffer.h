// File /Native/Engine/IO/IInputBuffer
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"

namespace PenEngine
{
	/// @brief  用于为输入输出设备提供用于写入数据的缓冲区
	class IInputBuffer
	{
	public:
		virtual ~IInputBuffer() noexcept = default;
		virtual U8* PrepareBuffer(Usize requiredLen) = 0;
		virtual void SetActualBufferLen(Usize actualLen) = 0;
	};
}