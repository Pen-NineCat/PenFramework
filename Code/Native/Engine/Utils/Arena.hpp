// File /Code/Native/Engine/Utils/Arena.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <deque>
#include <vector>

#include "../Core/Environment.h"

namespace PenEngine
{
	template <typename T, Usize N> requires std::is_constructible_v<T>
	class Arena
	{
	public:
		Arena() noexcept = default;
		~Arena() noexcept
		{
			for (auto& layer : m_layers)
			{
				if constexpr (!std::is_trivially_destructible_v<T>)
				{
					for (Usize i = 0; i < layer.Used; ++i)
						layer.Storage[i].~T();
				}

				operator delete(layer.Storage);
			}
		}

		Arena(const Arena& arena) = delete;
		Arena& operator=(const Arena& arena) = delete;

		Arena(Arena&& arena) noexcept
			: m_layers(std::move(arena.m_layers))
		{}

		Arena& operator=(Arena&& arena) noexcept
		{
			if (this != &arena)
			{
				m_layers = std::move(arena.m_layers);
			}
			return *this;
		}

		/// @brief 请求一个新的对象，如果无可用对象则会分配新的内存块
		T* Acquire()
		{
			// 遍历每一层，寻找未使用的块
			for (auto& layer : m_layers)
			{
				// 当前层有空闲块
				if (layer.Borrowed < layer.Used)
					return layer.Storage + (layer.Borrowed++);

				// 当前层无空闲块，但是有内存空间
				if (layer.Used < N)
				{
					new (layer.Storage + (layer.Used++)) T();
					return layer.Storage + (layer.Borrowed++);
				}
			}

			// 需要分配新的层
			Layer layer =
			{
				.Storage = static_cast<T*>(operator new(sizeof(T) * N)),
				.Used = 0,
				.Borrowed = 0
			};

			new (layer.Storage + (layer.Used++)) T();
			T* p = layer.Storage + (layer.Borrowed++);

			m_layers.push_back(layer);

			return p;
		}

		/// @brief 重置 Arena，标记所有对象为未使用状态，但不释放内存
		void Reset()
		{
			// 重置每一层
			for (auto& layer : m_layers)
			{
				// 如果当前层没有被借出块，说明后续的所有层也没有被借出，可以早退
				if (layer.Borrowed == 0)
					return;

				layer.Borrowed = 0;
			}
		}

		/// @brief 释放 Arena中所有未被使用的层的内存块，保留已使用的层
		void Free()
		{
			for (auto& layer : m_layers)
			{
				if (layer.Borrowed == 0)
				{
					if constexpr (!std::is_trivially_destructible_v<T>)
					{
						for (Usize i = 0; i < layer.Used; ++i)
							layer.Storage[i].~T();
					}

					operator delete(layer.Storage);
				}
			}
		}

		[[nodiscard]] Usize Capacity() const noexcept
		{
			Usize capaity = 0;
			for (auto& layer : m_layers)
				capaity += layer.Used;
			return capaity;
		}

		[[nodiscard]] Usize Size() const noexcept
		{
			Usize size = 0;
			for (auto& layer : m_layers)
				size += layer.Borrowed;
			return size;
		}
	private:
		struct Layer
		{
			T* Storage;
			Usize Used;
			Usize Borrowed;
		};

		std::vector<Layer> m_layers;
	};
}
