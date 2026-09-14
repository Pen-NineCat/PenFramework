// File /Native/Engine/String/StringView.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <iterator>

#include "../Core/Environment.h"

namespace PenEngine
{
	template <typename CharType>
	class StringConstIterator
	{
	public:
		using iterator_category = std::contiguous_iterator_tag;
		using iterator_concept = std::contiguous_iterator_tag;

		using value_type = CharType;
		using difference_type = PtrDiff;

		using const_pointer = const value_type*;
		using pointer = const_pointer;

		using const_reference = const value_type&;
		using reference = const_reference;

		constexpr StringConstIterator() noexcept = default;
		constexpr explicit StringConstIterator(pointer ptr) noexcept : m_ptr(ptr) {}

		constexpr reference operator*() const noexcept
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			return *m_ptr;
		}

		constexpr pointer operator->() const noexcept
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			return m_ptr;
		}

		constexpr StringConstIterator& operator++() noexcept
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			++m_ptr;
			return *this;
		}

		constexpr StringConstIterator operator++(int) noexcept
		{
			StringConstIterator tmp = *this;
			++(*this);
			return tmp;
		}

		constexpr StringConstIterator& operator--() noexcept
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			--m_ptr;
			return *this;
		}

		constexpr StringConstIterator operator--(int) noexcept
		{
			StringConstIterator tmp = *this;
			--(*this);
			return tmp;
		}

		constexpr StringConstIterator& operator+=(difference_type off)
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			m_ptr += off;
			return *this;
		}

		constexpr StringConstIterator& operator-=(difference_type off)
		{
			return (*this) += -off;
		}

		constexpr friend StringConstIterator operator+(StringConstIterator it, int n)
		{
			it += n;
			return it;
		}

		constexpr friend StringConstIterator operator+(int n, StringConstIterator it)
		{
			it += n;
			return it;
		}

		constexpr StringConstIterator operator-(difference_type off) const noexcept
		{
			StringConstIterator tmp = *this;
			tmp -= off;
			return tmp;
		}

		constexpr difference_type operator-(const StringConstIterator& it) const noexcept
		{
			DEBUG_VERIFY_REPORT(it.m_ptr && m_ptr, "cannot dereference value-initialized string iterator");
			return static_cast<difference_type>(m_ptr - it.m_ptr);
		}

		constexpr reference operator[](difference_type n) const noexcept
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			return m_ptr[n];
		}

		constexpr bool operator==(const StringConstIterator& other) const noexcept = default;
		constexpr auto operator<=>(const StringConstIterator& other) const noexcept = default;

		constexpr pointer Data() const noexcept
		{
			DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized string iterator");
			return m_ptr;
		}
	protected:
		pointer m_ptr;
	};

	template <typename CharType>
	class BasicStringView
	{
	public:
		using ReverseIterator = std::reverse_iterator<StringConstIterator<CharType>>;
		using ConstReverseIterator = std::reverse_iterator<StringConstIterator<CharType>>;
		using ConstIterator = StringConstIterator<CharType>;

		constexpr BasicStringView() noexcept = default;

		/*implicit*/ constexpr BasicStringView(std::nullptr_t) = delete;
		/*implicit*/ constexpr BasicStringView(const CharType* str) noexcept : m_str(str), m_size(std::char_traits<CharType>::lengtg(str)) {}
		constexpr BasicStringView(const CharType* str, Usize count) noexcept : m_str(str), m_size(count) {}
		constexpr BasicStringView(const CharType* begin, const CharType* end) noexcept : m_str(begin), m_size(end - begin) {}

		constexpr BasicStringView(ConstIterator begin, ConstIterator end) noexcept : m_str(begin.Data()), m_size(end - begin) {}

		constexpr BasicStringView(const BasicStringView&) noexcept = default;
		constexpr BasicStringView(BasicStringView&&) noexcept = default;
		constexpr BasicStringView& operator=(const BasicStringView&) noexcept = default;
		constexpr BasicStringView& operator=(BasicStringView&&) noexcept = default;

		constexpr ~BasicStringView() noexcept = default;
	private:
		CharType* m_str = nullptr;
		Usize m_size = 0;
	};

	using StringView = BasicStringView<char>;
	using WStringView = BasicStringView<wchar_t>;
	using U32StringView = BasicStringView<char32_t>;
}
