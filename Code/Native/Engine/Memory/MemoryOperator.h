// File /Native/Engine/Memory/MemoryOperator.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// 把主程序的全局 operator new/delete 接到 PenMemory.dll。
//
// 说明：
//   * 全局 operator new/delete 只能在一个翻译单元里定义，所以定义全部放在
//     MemoryOperator.cpp；本头文件只暴露接线所需的内部辅助函数。
//   * 这里接管的只是**主程序自己**的 operator new/delete（PenFramework.exe）。
//     其它模块（CRT、vcpkg 里的第三方 DLL）仍使用各自的堆，因此内存不要跨模块
//     「一边 new、另一边 delete」。
//   * PenMemory.dll 自身的元数据走 OSMemory 的 arena（VirtualAlloc / mmap），
//     不使用 operator new，所以不存在递归进入分配器的问题。

#pragma once
#include "Memory.hpp"

namespace PenEngine
{
	namespace Memory
	{
		namespace Detail
		{
			/// @brief operator new 系列的公共入口：申请 size 字节并按 alignment 对齐
			/// @param size 字节数
			/// @param alignment 对齐字节数，必须是 2 的幂
			/// @return 申请到的内存；失败返回 nullptr
			[[nodiscard]] void* AllocateForNew(Usize size, Usize alignment) noexcept;

			/// @brief operator new 系列申请失败时统一抛出 std::bad_alloc
			[[noreturn]] void ThrowBadAllocate();
		}
	}
}
