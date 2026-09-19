// File /Native/Engine/Memory/Memory.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../../Memory/Interface.h"
#include "../Core/Environment.h"
#include <cstddef>
#include <type_traits>

namespace PenEngine
{
	/// @brief PenMemory.dll（Code/Native/Memory）的 C++ 封装
	/// @note 返回的都是未构造的裸内存：本层不做 placement new，也不调用析构函数
	/// @note 类型模板重载可以省去手写 sizeof/alignof，例如 Memory::Allocate<Foo>(3)
	namespace Memory
	{
		/// @brief 按字节申请未构造内存
		/// @param size 字节数
		/// @return 申请到的内存；失败返回 nullptr
		[[nodiscard]] inline void* Allocate(Usize size) noexcept
		{
			return ::Allocate(size);
		}

		/// @brief 释放 Allocate / AlignedAllocate 返回的指针
		/// @param ptr 待释放指针，允许为 nullptr
		inline void Deallocate(void* ptr) noexcept
		{
			::Deallocate(ptr);
		}

		/// @brief 按字节申请对齐到 alignment 的未构造内存
		/// @param alignment 对齐字节数，必须是 2 的幂
		/// @param size 字节数
		/// @return 申请到的内存；失败返回 nullptr
		[[nodiscard]] inline void* AlignedAllocate(Usize alignment, Usize size) noexcept
		{
			return ::AlignedAllocate(alignment, size);
		}

		/// @brief 释放 AlignedAllocate 返回的指针
		/// @param ptr 待释放指针，允许为 nullptr
		/// @param alignment 申请时使用的对齐字节数，仅用于校验
		inline void AlignedDeallocate(void* ptr, Usize alignment) noexcept
		{
			::AlignedDeallocate(ptr, alignment);
		}

		/// @brief 申请 count 个 T 大小的未构造内存
		/// @tparam T 元素类型，用 sizeof(T) 计算总字节数
		/// @param count 元素个数
		/// @return 申请到的内存；失败返回 nullptr
		/// @note alignof(T) 大于分配器基础对齐（PEN_MEMORY_BASIC_ALIGNMENT）时自动走对齐分配路径，
		///       因此 alignas(16/32/...) 的类型也不会拿到错位的指针
		template <typename T>
		[[nodiscard]] T* Allocate(Usize count = 1) noexcept
		{
			static_assert(!std::is_void_v<T>, "Memory::Allocate<void> has no size");

			if constexpr (alignof(T) > static_cast<Usize>(PEN_MEMORY_BASIC_ALIGNMENT))
				return static_cast<T*>(::AlignedAllocate(alignof(T), sizeof(T) * count));
			else
				return static_cast<T*>(::Allocate(sizeof(T) * count));
		}

		/// @brief 申请 count 个 T 大小、且按 alignof(T) 对齐的未构造内存
		/// @tparam T 元素类型，用 sizeof(T) 与 alignof(T) 计算申请量
		/// @param count 元素个数
		/// @return 申请到的内存；失败返回 nullptr
		template <typename T>
		[[nodiscard]] T* AlignAllocate(Usize count = 1) noexcept
		{
			static_assert(!std::is_void_v<T>, "Memory::AlignAllocate<void> has no size");

			return static_cast<T*>(::AlignedAllocate(alignof(T), sizeof(T) * count));
		}

		/// @brief 释放 Allocate<T> / AlignAllocate<T> 返回的指针
		/// @tparam T 元素类型
		/// @param ptr 待释放指针，允许为 nullptr
		template <typename T>
		void Deallocate(T* ptr) noexcept
		{
			::Deallocate(const_cast<void*>(static_cast<const void*>(ptr)));
		}

		/// @brief 释放 AlignAllocate<T> 返回的指针，并用 alignof(T) 做对齐校验
		/// @tparam T 元素类型
		/// @param ptr 待释放指针，允许为 nullptr
		template <typename T>
		void AlignDeallocate(T* ptr) noexcept
		{
			::AlignedDeallocate(const_cast<void*>(static_cast<const void*>(ptr)), alignof(T));
		}

		/// @brief 查询一次分配的可用字节数；不属于本分配器的指针返回 0
		/// @param ptr 待查询指针
		[[nodiscard]] inline Usize GetAllocatedSize(const void* ptr) noexcept
		{
			return ::GetAllocatedSize(ptr);
		}

		/// @brief 查询 size 字节的申请实际会占用多少字节
		/// @param size 申请字节数
		[[nodiscard]] inline Usize GetEstimatedAllocatedSize(Usize size) noexcept
		{
			return ::GetEstimatedAllocatedSize(size);
		}

		/// @brief 读取一份堆统计快照
		/// @param stats 输出参数
		inline void GetStats(PenMemoryStats* stats) noexcept
		{
			::GetStats(stats);
		}

		/// @brief 把空闲页归还操作系统
		/// @return 实际归还的页数
		[[nodiscard]] inline Usize ReleaseFreeMemory() noexcept
		{
			return ::ReleaseFreeMemory();
		}

		/// @brief 分配器的页大小（8 KiB）
		[[nodiscard]] inline Usize GetPageSize() noexcept
		{
			return ::GetPageSize();
		}

		/// @brief Allocate 保证的基础对齐字节数
		[[nodiscard]] inline Usize GetAlignment() noexcept
		{
			return ::GetAlignment();
		}

		/// @brief 校验分配器全部内部不变量
		/// @param error 失败时指向一段静态错误描述，允许为 nullptr
		/// @return 一切正常返回 true
		[[nodiscard]] inline bool VerifyHeap(const char** error = nullptr) noexcept
		{
			return ::VerifyHeap(error) != 0;
		}
	}
}
