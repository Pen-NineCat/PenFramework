// File /Native/Engine/Memory/MemoryOperator.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// 让 PenMemory 成为主程序的 C++ 分配器：全局 operator new/delete 全套
// （含 sized / aligned / nothrow 变体）都转发到 PenMemory.dll。
//
// 这个文件属于 PenFramework.exe 目标，不会编进 PenMemory.dll：只替换主程序
// 自己的分配，不去接管其它模块的堆。

#include "MemoryOperator.h"

#include <cstddef>
#include <new>

namespace PenEngine
{
	namespace Memory
	{
		namespace Detail
		{
			void* AllocateForNew(Usize size, Usize alignment) noexcept
			{
				return ::AlignedAllocate(alignment, size);
			}

			void ThrowBadAllocate()
			{
				throw std::bad_alloc();
			}
		}
	}
}

namespace
{
	/// @brief 抛异常版本：申请失败时抛 std::bad_alloc
	[[nodiscard]] void* AllocateChecked(PenEngine::Usize size, PenEngine::Usize alignment)
	{
		void* ptr = PenEngine::Memory::Detail::AllocateForNew(size, alignment);
		if (ptr == nullptr)
			PenEngine::Memory::Detail::ThrowBadAllocate();
		return ptr;
	}

	/// @brief nothrow 版本：申请失败返回 nullptr
	[[nodiscard]] void* AllocateNothrow(PenEngine::Usize size, PenEngine::Usize alignment) noexcept
	{
		return PenEngine::Memory::Detail::AllocateForNew(size, alignment);
	}
}

// --- 抛异常形式 ------------------------------------------------------------
void* operator new(size_t size)
{
	return AllocateChecked(size, alignof(std::max_align_t));
}

void* operator new[](size_t size)
{
	return AllocateChecked(size, alignof(std::max_align_t));
}

// --- nothrow 形式 ----------------------------------------------------------
void* operator new(size_t size, const std::nothrow_t&) noexcept
{
	return AllocateNothrow(size, alignof(std::max_align_t));
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
	return AllocateNothrow(size, alignof(std::max_align_t));
}

// --- 过对齐形式 ------------------------------------------------------------
void* operator new(size_t size, std::align_val_t alignment)
{
	return AllocateChecked(size, static_cast<size_t>(alignment));
}

void* operator new[](size_t size, std::align_val_t alignment)
{
	return AllocateChecked(size, static_cast<size_t>(alignment));
}

void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return AllocateNothrow(size, static_cast<size_t>(alignment));
}

void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return AllocateNothrow(size, static_cast<size_t>(alignment));
}

// --- 释放 ------------------------------------------------------------------
void operator delete(void* ptr) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete[](void* ptr) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete(void* ptr, size_t) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete[](void* ptr, size_t) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete(void* ptr, std::align_val_t) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete(void* ptr, size_t, std::align_val_t) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete[](void* ptr, size_t, std::align_val_t) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { PenEngine::Memory::Deallocate(ptr); }
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { PenEngine::Memory::Deallocate(ptr); }
