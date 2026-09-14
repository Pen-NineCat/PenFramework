// File /Native/Engine/String/String.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "StringView.hpp"
#include <limits>
#include <string>

namespace PenEngine
{
	template <typename CharType>
	class BasicString
	{
	public:
		constexpr static U8 LocalStorageCapacity = (16ull / sizeof(CharType)) - 1;
		constexpr static U8 AllocateMask = sizeof(CharType) <= 1 ? 15
			: sizeof(CharType) <= 2 ? 7
			: sizeof(CharType) <= 4 ? 3
			: sizeof(CharType) <= 8 ? 1
			: 0;
		constexpr static Usize MaxStorageCapacity = (std::numeric_limits<Usize>::max() >> 1) / sizeof(CharType);
		constexpr static Usize NPos = static_cast<Usize>(-1);

		class Iterator : public StringConstIterator<CharType>
		{
		public:
			using iterator_category = std::contiguous_iterator_tag;
			using value_type = CharType;
			using difference_type = PtrDiff;
			using pointer = CharType*;
			using reference = CharType&;

			constexpr Iterator() noexcept : StringConstIterator<CharType>()
			{}
			explicit constexpr Iterator(pointer ptr) noexcept : StringConstIterator<CharType>(ptr)
			{}

			reference operator*() const noexcept
			{
				return *const_cast<pointer>(this->m_ptr);
			}
			pointer operator->() const noexcept
			{
				return const_cast<pointer>(this->m_ptr);
			}

			Iterator& operator++() noexcept
			{
				++this->m_ptr; return *this;
			}
			Iterator operator++(int) noexcept
			{
				Iterator tmp = *this; ++(*this); return tmp;
			}
			Iterator& operator--() noexcept
			{
				--this->m_ptr; return *this;
			}
			Iterator operator--(int) noexcept
			{
				Iterator tmp = *this; --(*this); return tmp;
			}

			Iterator& operator+=(difference_type n) noexcept
			{
				this->m_ptr += n; return *this;
			}
			Iterator& operator-=(difference_type n) noexcept
			{
				this->m_ptr -= n; return *this;
			}

			friend Iterator operator+(Iterator it, difference_type n) noexcept
			{
				it += n; return it;
			}
			friend Iterator operator+(difference_type n, Iterator it) noexcept
			{
				it += n; return it;
			}
			friend Iterator operator-(Iterator it, difference_type n) noexcept
			{
				it -= n; return it;
			}

			difference_type operator-(const Iterator& it) const noexcept
			{
				return static_cast<difference_type>(this->m_ptr - it.m_ptr);
			}

			reference operator[](difference_type n) const noexcept
			{
				return const_cast<reference>(this->m_ptr[n]);
			}

			bool operator==(const Iterator&) const noexcept = default;
			auto operator<=>(const Iterator&) const noexcept = default;
		};
		using ConstIterator = StringConstIterator<CharType>;
		using ReverseIterator = std::reverse_iterator<Iterator>;
		using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

		BasicString() noexcept;
		explicit BasicString(Usize initialCapacity);

		explicit BasicString(std::nullptr_t) noexcept = delete;

		BasicString(const BasicString& str);
		/* implicit */ BasicString(const CharType* str);
		/* implicit */ BasicString(const std::basic_string<CharType>& str);
		/* implicit */ BasicString(std::basic_string_view<CharType> str);

		BasicString(const BasicString& str, Usize pos, Usize count);
		BasicString(const CharType* str, Usize count);
		BasicString(const std::basic_string<CharType>& str, Usize pos, Usize count);
		BasicString(std::basic_string_view<CharType> str, Usize pos, Usize count);

		/* implicit */ operator std::basic_string<CharType>();
		/* implicit */ operator std::basic_string_view<CharType>();

		[[nodiscard]] Usize Size() const noexcept;
		[[nodiscard]] Usize Capacity() const noexcept;
		[[nodiscard]] CharType* Data() noexcept;
		[[nodiscard]] const CharType* Data() const noexcept;

		template <typename TargetCharType>
		BasicString<TargetCharType> ConvertToString();
		template <typename TargetCharType>
		std::basic_string<TargetCharType> ConvertToStdString();

		// 以下函数或using声明为专为STL算法提供的小写版本
		using value_type = CharType;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = value_type*;
		using const_pointer = const value_type*;

		using size_type = Usize;
		using difference_type = PtrDiff;

		using is_contiguous = std::true_type;

		using iterator = Iterator;
		using const_iterator = ConstIterator;
		using reverse_iterator = ReverseIterator;
		using const_reverse_iterator = ConstReverseIterator;

		[[nodiscard]] pointer data() noexcept;
		[[nodiscard]] const_pointer data() const noexcept;

		[[nodiscard]] size_t size() const noexcept;
		[[nodiscard]] size_t capacity() const noexcept;

		[[nodiscard]] iterator begin() noexcept;
		[[nodiscard]] iterator end() noexcept;

		[[nodiscard]] reverse_iterator rbegin() noexcept;
		[[nodiscard]] reverse_iterator rend() noexcept;

		[[nodiscard]] const_iterator cbegin() const noexcept;
		[[nodiscard]] const_iterator cend() const noexcept;

		[[nodiscard]] const_reverse_iterator crbegin() noexcept;
		[[nodiscard]] const_reverse_iterator crend() noexcept;

		iterator insert(iterator pos, CharType value);
		iterator erase(iterator pos);
		iterator erase(iterator begin, iterator end);

		void push_back(CharType value);
		void clear() noexcept;
	private:
		static Usize CalculateAllocateCapacity(Usize requestCapacity, Usize currentCapacity, Usize maxCapacity) noexcept;

		void InitLocalBuffer() noexcept;
		void InitHeapBuffer(Usize initialCapacity);

		void ReallocateBuffer(Usize capacity);
		void ReallocateBufferByCapacity(Usize capacity);

		void MoveToLocal();

		union StringBuffer
		{
			CharType Stack[LocalStorageCapacity];
			CharType* Heap;
		};

		StringBuffer m_buffer;
		Usize m_capacity = 0;
		Usize m_size = 0;
	};

	template <typename CharType>
	BasicString<CharType>::BasicString() noexcept
	{
		InitLocalBuffer();
	}

	template <typename CharType>
	BasicString<CharType>::BasicString(Usize initialCapacity)
	{
		if (initialCapacity <= LocalStorageCapacity)
			InitLocalBuffer();
		else
			InitHeapBuffer(initialCapacity);
	}

	template <typename CharType>
	BasicString<CharType>::BasicString(const BasicString& str) : BasicString(str.Data(), str.Size())
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(const CharType* str) : BasicString(str, std::char_traits<CharType>::length(str)) {}

	template <typename CharType>
	BasicString<CharType>::BasicString(const std::basic_string<CharType>& str) : BasicString(str.data(),str.size())
	{
	}

	template <typename CharType>
	BasicString<CharType>::BasicString(std::basic_string_view<CharType> str) : BasicString(str.data(), str.size())
	{
	}

	template <typename CharType>
	BasicString<CharType>::BasicString(const CharType* str, Usize count)
	{
		if (count <= LocalStorageCapacity)
		{
			InitLocalBuffer();
			std::char_traits<CharType>::copy(m_buffer.Stack, str, count);
			m_buffer.Stack[count] = CharType();
			return;
		}

		InitHeapBuffer(count);
		std::char_traits<CharType>::copy(m_buffer.Heap, str, count);
		m_buffer.Heap[count] = CharType();
		return;
	}

	template <typename CharType>
	BasicString<CharType>::BasicString(const std::basic_string<CharType>& str, Usize pos, Usize count) : BasicString(str.data() + pos, count)
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(std::basic_string_view<CharType> str, Usize pos, Usize count) : BasicString(str.data() + pos, count)
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(const BasicString& str, Usize pos, Usize count) : BasicString(str.Data() + pos, count)
	{}

	template <typename CharType>
	BasicString<CharType>::operator std::basic_string<CharType>()
	{
		return std::basic_string<CharType>(Data(), Size());
	}

	template <typename CharType>
	BasicString<CharType>::operator std::basic_string_view<CharType>()
	{
		return std::basic_string<CharType>(Data(), Size());
	}

	template <typename CharType>
	Usize BasicString<CharType>::Size() const noexcept
	{
		return m_size;
	}

	template <typename CharType>
	Usize BasicString<CharType>::Capacity() const noexcept
	{
		return m_capacity;
	}

	template <typename CharType>
	CharType* BasicString<CharType>::Data() noexcept
	{
		return m_capacity <= LocalStorageCapacity ? m_buffer.Stack : m_buffer.Heap;
	}

	template <typename CharType>
	const CharType* BasicString<CharType>::Data() const noexcept
	{
		return m_capacity <= LocalStorageCapacity ? m_buffer.Stack : m_buffer.Heap;
	}

	template <typename CharType>
	BasicString<CharType>::pointer BasicString<CharType>::data() noexcept
	{
		return Data();
	}

	template <typename CharType>
	BasicString<CharType>::const_pointer BasicString<CharType>::data() const noexcept
	{
		return Data();
	}

	template <typename CharType>
	size_t BasicString<CharType>::size() const noexcept
	{
		return Size();
	}

	template <typename CharType>
	size_t BasicString<CharType>::capacity() const noexcept
	{
		return Capacity();
	}

	template <typename CharType>
	Usize BasicString<CharType>::CalculateAllocateCapacity(Usize requestCapacity, Usize currentCapacity,
		Usize maxCapacity) noexcept
	{
		Usize masked = requestCapacity | AllocateMask;
		if (masked > maxCapacity)
			return maxCapacity;

		if (currentCapacity > maxCapacity - currentCapacity / 2)
			return maxCapacity;

		return std::max(masked, currentCapacity + currentCapacity / 2);
	}

	template <typename CharType>
	void BasicString<CharType>::InitLocalBuffer() noexcept
	{
		m_capacity = LocalStorageCapacity;
		m_size = 0;
		m_buffer.Stack[0] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::InitHeapBuffer(Usize initialCapacity)
	{
		initialCapacity = CalculateAllocateCapacity(initialCapacity, LocalStorageCapacity, MaxStorageCapacity);

		CharType* buffer = new CharType[initialCapacity + 1];

		m_size = 0;
		m_capacity = initialCapacity;
		m_buffer.Heap = buffer;
		m_buffer.Heap[0] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::ReallocateBuffer(Usize capacity)
	{
		ReallocateBufferByCapacity(CalculateAllocateCapacity(capacity,m_capacity,MaxStorageCapacity));
	}

	template <typename CharType>
	void BasicString<CharType>::ReallocateBufferByCapacity(Usize capacity)
	{
		CharType* newBuffer = new CharType[capacity + 1];

		// 不需要m_size > 0判断，copy会检查，并且就算为0并且触发了复制，其也不会造成副作用，因为m_buffer.Heap是有效的
		std::char_traits<CharType>::copy(newBuffer, m_buffer.Heap, m_size);

		delete m_buffer.Heap;

		m_buffer.Heap = newBuffer;
		m_capacity = capacity;
		m_buffer.Heap[m_size] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::MoveToLocal()
	{		
		// 这一步对m_size的检查应该在调用前进行，这里不做防御性检查
		CharType* heapBuffer = m_buffer.Heap;

		// 如果需要constexpr路径，需要先初始化StackBuffer
		// 并且InitSSOBuffer()需要在if consteval路径下构造每个元素

		std::char_traits<CharType>::copy(m_buffer.Stack, heapBuffer, m_size);

		delete heapBuffer;

		m_capacity = LocalStorageCapacity;

		// 可以通过在copy中复制size + 1个元素来复制结尾符，但是为了规范性，仍然采用手动设置结尾符
		m_buffer.Stack[m_size] = CharType();
	}

	using String = BasicString<char>;
	using WString = BasicString<wchar_t>;
	using U32String = BasicString<char32_t>;
}
