// File /Native/Engine/String/String.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../Exception/Exception.hpp"
#include "../Utils/Concept.hpp"
#include "StringView.hpp"
#include <algorithm>
#include <limits>
#include <string>
#include <boost/locale/encoding_utf.hpp>

namespace PenEngine
{
	// BasicString 可接受的“字符串类”区间：同字符类型的连续定长区间
	template <typename CharType, typename Range>
	concept IsBasicStringRange = std::ranges::contiguous_range<Range>
		&& std::ranges::sized_range<Range>
		&& std::same_as<std::ranges::range_value_t<Range>, CharType>
		&& !std::same_as<std::remove_cvref_t<Range>, std::basic_string<CharType>>
		&& !std::same_as<std::remove_cvref_t<Range>, std::basic_string_view<CharType>>
		&& !std::is_convertible_v<Range, const CharType*>;

	class BadUTFConvertException : public Exception
	{
	public:
		explicit BadUTFConvertException(std::stacktrace stacktrace = std::stacktrace::current(), const std::source_location& sourceLocation = std::source_location::current())
			: Exception("BadUTFConvertException", "无法识别可能的UTF编码字符", std::move(stacktrace), sourceLocation)
		{}
	};

	class BadNumberConvertException : public Exception
	{
	public:
		explicit BadNumberConvertException(std::stacktrace stacktrace = std::stacktrace::current(), const std::source_location& sourceLocation = std::source_location::current())
			: Exception("BadNumberConvertException", "无法将数字转为字符串", std::move(stacktrace), sourceLocation)
		{}
	};

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
		/* implicit */ BasicString(BasicStringView<CharType> str);
		/* implicit */ BasicString(const CharType* str);
		/* implicit */ BasicString(const std::basic_string<CharType>& str);
		/* implicit */ BasicString(std::basic_string_view<CharType> str);

		BasicString(const BasicString& str, Usize pos, Usize count);
		BasicString(StringView str, Usize pos, Usize count);
		BasicString(const CharType* str, Usize count);
		BasicString(const std::basic_string<CharType>& str, Usize pos, Usize count);
		BasicString(std::basic_string_view<CharType> str, Usize pos, Usize count);

		BasicString(BasicString&& other) noexcept;
		BasicString& operator=(const BasicString& str);
		void DeallocateBuffer() noexcept;
		BasicString& operator=(BasicString&& other) noexcept;

		/* implicit */ [[nodiscard]] operator std::basic_string<CharType>() const;
		/* implicit */ [[nodiscard]] operator std::basic_string_view<CharType>() const noexcept;
		/* implicit */ [[nodiscard]] operator BasicStringView<CharType>() const noexcept;

		BasicString& operator=(BasicStringView<CharType> str);
		BasicString& operator=(const CharType* str);
		BasicString& operator=(const std::basic_string<CharType>& str);
		BasicString& operator=(std::basic_string_view<CharType> str);
		BasicString& operator=(CharType ch);

		[[nodiscard]] CharType& operator[](Usize pos) noexcept;
		[[nodiscard]] CharType operator[](Usize pos) const noexcept;

		[[nodiscard]] Usize Size() const noexcept;
		[[nodiscard]] Usize Capacity() const noexcept;
		[[nodiscard]] CharType* Data() noexcept;
		[[nodiscard]] const CharType* Data() const noexcept;
		[[nodiscard]] CharType* DataEnd() noexcept;
		[[nodiscard]] const CharType* DataEnd() const noexcept;

		template <typename TargetCharType>
		BasicString<TargetCharType> ConvertToString() const;
		template <typename TargetCharType>
		std::basic_string<TargetCharType> ConvertToStdString() const;

		void Reserve(Usize capacity);
		void Resize(Usize size);
		void Resize(Usize size, CharType fill);

		BasicString<CharType> Substr(Usize pos, Usize len = NPos) const;
		BasicStringView<CharType> Subview(Usize pos, Usize len = NPos) const noexcept;

		void ShrinkToFit();
		void RequestToFit();

		bool Empty() const noexcept;
		void Clear() noexcept;

		void Replace(CharType from, CharType to, Usize off = 0) noexcept;
		void Replace(BasicStringView<CharType> from, BasicStringView<CharType> to, Usize off = 0);
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

		[[nodiscard]] const_iterator begin() const noexcept;
		[[nodiscard]] const_iterator end() const noexcept;

		[[nodiscard]] reverse_iterator rbegin() noexcept;
		[[nodiscard]] reverse_iterator rend() noexcept;

		[[nodiscard]] const_reverse_iterator rbegin() const noexcept;
		[[nodiscard]] const_reverse_iterator rend() const noexcept;

		[[nodiscard]] const_iterator cbegin() const noexcept;
		[[nodiscard]] const_iterator cend() const noexcept;

		[[nodiscard]] const_reverse_iterator crbegin() noexcept;
		[[nodiscard]] const_reverse_iterator crend() noexcept;

		iterator insert(iterator pos, CharType value);
		iterator erase(iterator pos);
		iterator erase(iterator begin, iterator end);

		void push_back(CharType value) { PushBack(value); }

		[[nodiscard]] constexpr bool StartWith(CharType ch) const noexcept;
		[[nodiscard]] constexpr bool StartWith(const CharType* str) const noexcept;
		[[nodiscard]] constexpr bool StartWith(BasicStringView<CharType> str) const noexcept;
		[[nodiscard]] constexpr bool StartWith(std::basic_string_view<CharType> str) const noexcept;
		[[nodiscard]] constexpr bool StartWith(const std::basic_string<CharType>& str) const noexcept;
		[[nodiscard]] constexpr bool StartWith(const BasicString& str) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr bool StartWith(const Range& str) const noexcept;

		[[nodiscard]] constexpr bool EndWith(CharType ch) const noexcept;
		[[nodiscard]] constexpr bool EndWith(const CharType* str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(BasicStringView<CharType> str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(std::basic_string_view<CharType> str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(const std::basic_string<CharType>& str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(const BasicString& str) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr bool EndWith(const Range& str) const noexcept;

		[[nodiscard]] constexpr Usize Find(CharType ch, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const CharType* str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(BasicStringView<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(std::basic_string_view<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const std::basic_string<CharType>& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const BasicString& str, Usize pos = 0) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr Usize Find(const Range& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize RFind(CharType ch, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const CharType* str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(BasicStringView<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(std::basic_string_view<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const std::basic_string<CharType>& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const BasicString& str, Usize pos = NPos) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr Usize RFind(const Range& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindFirstOf(CharType ch, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const CharType* str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(BasicStringView<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(std::basic_string_view<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const std::basic_string<CharType>& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const BasicString& str, Usize pos = 0) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindFirstOf(const Range& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindFirstNotOf(CharType ch, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const CharType* str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(BasicStringView<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(std::basic_string_view<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const std::basic_string<CharType>& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const BasicString& str, Usize pos = 0) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindFirstNotOf(const Range& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindLastOf(CharType ch, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const CharType* str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(BasicStringView<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(std::basic_string_view<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const std::basic_string<CharType>& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const BasicString& str, Usize pos = NPos) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindLastOf(const Range& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindLastNotOf(CharType ch, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const CharType* str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(BasicStringView<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(std::basic_string_view<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const std::basic_string<CharType>& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const BasicString& str, Usize pos = NPos) const noexcept;
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindLastNotOf(const Range& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const CharType* str, Usize size, Usize pos) const noexcept;

		BasicString& PushBack(CharType ch);
		BasicString& PushBack(const CharType* str);
		BasicString& PushBack(BasicStringView<CharType> str);
		BasicString& PushBack(std::basic_string_view<CharType> str);
		BasicString& PushBack(const std::basic_string<CharType>& str);
		BasicString& PushBack(const BasicString& str);
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			BasicString& PushBack(const Range& str);
		BasicString& PushBack(const CharType* str, Usize size);

		BasicString& operator+=(CharType ch);
		BasicString& operator+=(const CharType* str);
		BasicString& operator+=(BasicStringView<CharType> str);
		BasicString& operator+=(std::basic_string_view<CharType> str);
		BasicString& operator+=(const std::basic_string<CharType>& str);
		BasicString& operator+=(const BasicString& str);
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			BasicString& operator+=(const Range& str);

		void PushFront(const BasicString& other);
		void PushFront(const CharType* str);
		void PushFront(const CharType* str, Usize size);
		void PushFront(CharType ch, Usize count = 1);
		void PushFront(const std::basic_string<CharType>& str);
		void PushFront(std::basic_string_view<CharType> str);
		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			void PushFront(const Range& str);

		template <typename SourceCharType>
		void ConvertAndPushBack(const BasicString<SourceCharType>& str);
		template <typename SourceCharType>
		void ConvertAndPushBack(BasicStringView<SourceCharType> str);
		template <typename SourceCharType>
		void ConvertAndPushBack(const SourceCharType* str);
		template <typename SourceCharType>
		void ConvertAndPushBack(const SourceCharType* str, Usize size);
		template <typename SourceCharType>
		void ConvertAndPushBack(const std::basic_string<SourceCharType>& str);
		template <typename SourceCharType>
		void ConvertAndPushBack(std::basic_string_view<SourceCharType> str);
		template <typename SourceCharType>
		void ConvertAndPushBack(SourceCharType ch);
		template <typename SourceCharType, typename Range> requires(IsBasicStringRange<SourceCharType, Range>)
			void ConvertAndPushBack(const Range& str);
		template <typename T> requires (std::is_arithmetic_v<T> && !IsOneOf<T, char, wchar_t, char8_t, char16_t, char32_t>)
			void ConvertAndPushBack(T v);

		template <typename SourceCharType>
		void ConvertAndPushFront(const BasicString<SourceCharType>& str);
		template <typename SourceCharType>
		void ConvertAndPushFront(BasicStringView<SourceCharType> str);
		template <typename SourceCharType>
		void ConvertAndPushFront(const SourceCharType* str);
		template <typename SourceCharType>
		void ConvertAndPushFront(const SourceCharType* str, Usize size);
		template <typename SourceCharType>
		void ConvertAndPushFront(const std::basic_string<SourceCharType>& str);
		template <typename SourceCharType>
		void ConvertAndPushFront(std::basic_string_view<SourceCharType> str);
		template <typename SourceCharType, typename Range> requires(IsBasicStringRange<SourceCharType, Range>)
			void ConvertAndPushFront(const Range& str);
		template <typename T> requires (std::is_arithmetic_v<T> && !IsOneOf<T, char, wchar_t, char8_t, char16_t, char32_t>)
			void ConvertAndPushFront(T v);

		template <typename SourceCharType>
		void ConvertFrom(const BasicString<SourceCharType>& str);
		template <typename SourceCharType>
		void ConvertFrom(BasicStringView<SourceCharType> str);
		template <typename SourceCharType>
		void ConvertFrom(const SourceCharType* str);
		template <typename SourceCharType>
		void ConvertFrom(const SourceCharType* str, Usize size);
		template <typename SourceCharType>
		void ConvertFrom(const std::basic_string<SourceCharType>& str);
		template <typename SourceCharType>
		void ConvertFrom(std::basic_string_view<SourceCharType> str);
		template <typename SourceCharType, typename Range> requires(IsBasicStringRange<SourceCharType, Range>)
			void ConvertFrom(const Range& str);
		template <typename T> requires (std::is_arithmetic_v<T> && !IsOneOf<T, char, wchar_t, char8_t, char16_t, char32_t>)
			void ConvertFrom(T v);

		void DeallocateAndRebuild(const CharType* str, Usize len);
		void DeallocateAndRebuild(CharType ch, Usize count);

		[[nodiscard]] friend BasicString operator+(const BasicString& lhs, const BasicString& rhs)
		{
			BasicString result(lhs.Size() + rhs.Size());
			result += lhs;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(const BasicString& lhs, CharType ch)
		{
			BasicString result(lhs.Size() + 1);
			result += lhs;
			result += ch;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(CharType ch, const BasicString& rhs)
		{
			BasicString result(rhs.Size() + 1);
			result += ch;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(const BasicString& lhs, BasicStringView<CharType> rhs)
		{
			BasicString result(lhs.Size() + rhs.Size());
			result += lhs;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(BasicStringView<CharType> lhs, const BasicString& rhs)
		{
			BasicString result(lhs.Size() + rhs.Size());
			result += lhs;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(const BasicString& lhs, const CharType* rhs)
		{
			const BasicStringView<CharType> rhsView(rhs);
			return lhs + rhsView;
		}

		[[nodiscard]] friend BasicString operator+(const CharType* lhs, const BasicString& rhs)
		{
			const BasicStringView<CharType> lhsView(lhs);
			return lhsView + rhs;
		}

		[[nodiscard]] friend BasicString operator+(const BasicString& lhs, std::basic_string_view<CharType> rhs)
		{
			BasicString result(lhs.Size() + rhs.size());
			result += lhs;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(std::basic_string_view<CharType> lhs, const BasicString& rhs)
		{
			BasicString result(lhs.size() + rhs.Size());
			result += lhs;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(const BasicString& lhs, const std::basic_string<CharType>& rhs)
		{
			BasicString result(lhs.Size() + rhs.size());
			result += lhs;
			result += rhs;
			return result;
		}

		[[nodiscard]] friend BasicString operator+(const std::basic_string<CharType>& lhs, const BasicString& rhs)
		{
			BasicString result(lhs.size() + rhs.Size());
			result += lhs;
			result += rhs;
			return result;
		}

		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] friend BasicString operator+(const BasicString& lhs, const Range& rhs)
		{
			const BasicStringView<CharType> rhsView(rhs);
			return lhs + rhsView;
		}

		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] friend BasicString operator+(const Range& lhs, const BasicString& rhs)
		{
			const BasicStringView<CharType> lhsView(lhs);
			return lhsView + rhs;
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const BasicString& lhs,
			const BasicString& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.Data(), lhs.m_size, rhs.Data(), rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const BasicString& lhs,
			BasicStringView<CharType> rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.Data(), lhs.m_size, rhs.Data(), rhs.Size());
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(BasicStringView<CharType> lhs,
			const BasicString& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.Data(), lhs.Size(), rhs.Data(), rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const BasicString& lhs,
			const CharType* rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.Data(), lhs.m_size, rhs,
				std::char_traits<CharType>::length(rhs));
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const CharType* lhs,
			const BasicString& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs, std::char_traits<CharType>::length(lhs),
				rhs.Data(), rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const BasicString& lhs,
			std::basic_string_view<CharType> rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.Data(), lhs.m_size, rhs.data(), rhs.size());
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(std::basic_string_view<CharType> lhs,
			const BasicString& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.data(), lhs.size(), rhs.Data(), rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const BasicString& lhs,
			const std::basic_string<CharType>& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.Data(), lhs.m_size, rhs.data(), rhs.size());
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const std::basic_string<CharType>& lhs,
			const BasicString& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.data(), lhs.size(), rhs.Data(), rhs.m_size);
		}

		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const BasicString& lhs,
				const Range& rhs) noexcept
		{
			const BasicStringView<CharType> rhsView(rhs);
			return StringCompareDetail::Compare(lhs.Data(), lhs.m_size, rhsView.Data(), rhsView.Size());
		}

		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const Range& lhs,
				const BasicString& rhs) noexcept
		{
			const BasicStringView<CharType> lhsView(lhs);
			return StringCompareDetail::Compare(lhsView.Data(), lhsView.Size(), rhs.Data(), rhs.m_size);
		}

		[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs, const BasicString& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs,
			BasicStringView<CharType> rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(BasicStringView<CharType> lhs,
			const BasicString& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs, const CharType* rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const CharType* lhs, const BasicString& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs, CharType rhs) noexcept
		{
			return lhs.m_size == 1 && lhs.Data()[0] == rhs;
		}

		[[nodiscard]] friend constexpr bool operator==(CharType lhs, const BasicString& rhs) noexcept
		{
			return rhs.m_size == 1 && rhs.Data()[0] == lhs;
		}

		[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs,
			std::basic_string_view<CharType> rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(std::basic_string_view<CharType> lhs,
			const BasicString& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs,
			const std::basic_string<CharType>& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const std::basic_string<CharType>& lhs,
			const BasicString& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] friend constexpr bool operator==(const BasicString& lhs, const Range& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		template <typename Range> requires(IsBasicStringRange<CharType, Range>)
			[[nodiscard]] friend constexpr bool operator==(const Range& lhs, const BasicString& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}
	private:
		static Usize CalculateAllocateCapacity(Usize requestCapacity, Usize currentCapacity, Usize maxCapacity) noexcept;

		void InitLocalBuffer() noexcept;
		void InitHeapBuffer(Usize initialCapacity);

		void ReallocateHeapBuffer(Usize capacity);
		void ReallocateHeapBufferByCapacity(Usize capacity);

		void MoveToHeap(Usize newCapacity);
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
	BasicString<CharType>::BasicString(BasicStringView<CharType> str) : BasicString(str.Data(), str.Size())
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(const CharType* str) : BasicString(str, std::char_traits<CharType>::length(str)) {}

	template <typename CharType>
	BasicString<CharType>::BasicString(const std::basic_string<CharType>& str) : BasicString(str.data(), str.size())
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(std::basic_string_view<CharType> str) : BasicString(str.data(), str.size())
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(const CharType* str, Usize count)
	{
		if (count <= LocalStorageCapacity)
		{
			InitLocalBuffer();
			std::char_traits<CharType>::copy(m_buffer.Stack, str, count);
			m_size = count;
			m_buffer.Stack[count] = CharType();
			return;
		}

		InitHeapBuffer(count);
		std::char_traits<CharType>::copy(m_buffer.Heap, str, count);
		m_size = count;
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
	BasicString<CharType>::BasicString(BasicString&& other) noexcept
	{
		InitLocalBuffer();
		std::swap(m_buffer, other.m_buffer);
		std::swap(m_size, other.m_size);
		std::swap(m_capacity, other.m_capacity);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(const BasicString& str)
	{
		Clear();
		PushBack(str);
		return *this;
	}

	template <typename CharType>
	void BasicString<CharType>::DeallocateBuffer() noexcept
	{
		if (m_capacity > LocalStorageCapacity)
			delete[] m_buffer.Heap;
		InitLocalBuffer();
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(BasicString&& other) noexcept
	{
		if (&other == this)
			return *this;

		DeallocateBuffer();
		std::swap(m_buffer, other.m_buffer);
		std::swap(m_size, other.m_size);
		std::swap(m_capacity, other.m_capacity);
		return *this;
	}

	template <typename CharType>
	BasicString<CharType>::BasicString(const BasicString& str, Usize pos, Usize count) : BasicString(str.Data() + pos, count)
	{}

	template <typename CharType>
	BasicString<CharType>::BasicString(StringView str, Usize pos, Usize count) : BasicString(str.Data() + pos, count)
	{}

	template <typename CharType>
	BasicString<CharType>::operator std::basic_string<CharType>() const
	{
		return std::basic_string<CharType>(Data(), Size());
	}

	template <typename CharType>
	BasicString<CharType>::operator std::basic_string_view<CharType>() const noexcept
	{
		return std::basic_string_view<CharType>(Data(), Size());
	}

	template <typename CharType>
	BasicString<CharType>::operator BasicStringView<CharType>() const noexcept
	{
		return BasicStringView<CharType>(Data(), Size());
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(BasicStringView<CharType> str)
	{
		Clear();
		PushBack(str);
		return *this;
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(const CharType* str)
	{
		Clear();
		PushBack(str);
		return *this;
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(const std::basic_string<CharType>& str)
	{
		Clear();
		PushBack(str);
		return *this;
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(std::basic_string_view<CharType> str)
	{
		Clear();
		PushBack(str);
		return *this;
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator=(CharType ch)
	{
		Clear();
		PushBack(ch);
		return *this;
	}

	template <typename CharType>
	CharType& BasicString<CharType>::operator[](Usize pos) noexcept
	{
		DEBUG_VERIFY_REPORT(pos < m_size, "Invalid Argument");
		return Data()[pos];
	}

	template <typename CharType>
	CharType BasicString<CharType>::operator[](Usize pos) const noexcept
	{
		DEBUG_VERIFY_REPORT(pos < m_size, "Invalid Argument");
		return Data()[pos];
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
	CharType* BasicString<CharType>::DataEnd() noexcept
	{
		return Data() + m_size;
	}

	template <typename CharType>
	const CharType* BasicString<CharType>::DataEnd() const noexcept
	{
		return Data() + m_size;
	}

	template <typename CharType>
	template <typename TargetCharType>
	BasicString<TargetCharType> BasicString<CharType>::ConvertToString() const
	{
		BasicString<TargetCharType> tmp;
		tmp.ConvertFrom(*this);
		return tmp;
	}

	template <typename CharType>
	template <typename TargetCharType>
	std::basic_string<TargetCharType> BasicString<CharType>::ConvertToStdString() const
	{
		return boost::locale::conv::utf_to_utf<TargetCharType, CharType>(Data(), DataEnd());
	}

	template <typename CharType>
	void BasicString<CharType>::Reserve(Usize capacity)
	{
		if (capacity <= m_capacity)
			return;

		if (m_capacity > LocalStorageCapacity)
			ReallocateHeapBuffer(capacity);
		else
			MoveToHeap(capacity);
	}

	template <typename CharType>
	void BasicString<CharType>::Resize(Usize size)
	{
		if (Usize currentSize = Size(); size > currentSize)
		{
			if (Capacity() < size)
				Reserve(size);
			CharType* buffer = Data();

			buffer[currentSize + size] = CharType();
		}

		m_size = size;
	}

	template <typename CharType>
	void BasicString<CharType>::Resize(Usize size, CharType fill)
	{
		if (Usize currentSize = Size(); size > currentSize)
		{
			if (Capacity() < size)
				Reserve(size);
			CharType* buffer = Data();

			std::char_traits<CharType>::assign(buffer + currentSize, size, fill);

			buffer[currentSize + size] = CharType();
		}

		m_size = size;
	}

	template <typename CharType>
	BasicString<CharType> BasicString<CharType>::Substr(Usize pos, Usize len) const
	{
		return Subview(pos, len);
	}

	template <typename CharType>
	BasicStringView<CharType> BasicString<CharType>::Subview(Usize pos, Usize len) const noexcept
	{
		return BasicStringView<CharType>(Data() + pos, std::min(len, Size() - pos));
	}

	template <typename CharType>
	void BasicString<CharType>::ShrinkToFit()
	{
		if (m_capacity > LocalStorageCapacity)
		{
			Usize waste = Capacity() - Size();
			if (waste <= Size() * 1.5 && waste <= 512)
				return;

			Usize actualFitCapacity = Size() * 5 / 4;

			if (actualFitCapacity <= LocalStorageCapacity)
				MoveToLocal();
			else
				ReallocateHeapBufferByCapacity(actualFitCapacity);
		}
	}

	template <typename CharType>
	void BasicString<CharType>::RequestToFit()
	{
		if (m_capacity > LocalStorageCapacity)
		{
			if (m_size <= LocalStorageCapacity)
				MoveToLocal();
			else if (m_size < m_capacity)
				ReallocateHeapBufferByCapacity(m_size);
		}
	}

	template <typename CharType>
	bool BasicString<CharType>::Empty() const noexcept
	{
		return m_size == 0;
	}

	template <typename CharType>
	void BasicString<CharType>::Clear() noexcept
	{
		m_size = 0;
		Data()[0] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::Replace(CharType from, CharType to, Usize off) noexcept
	{
		if (off >= Size())
			return;

		std::replace(Data() + off, Data() + Size(), from, to);
	}

	template <typename CharType>
	void BasicString<CharType>::Replace(BasicStringView<CharType> from, BasicStringView<CharType> to, Usize off)
	{
		if (from.Empty() || from.Size() + off > Size() || from == to)
			return;

		Usize pos = Find(from, off);
		if (pos == NPos)
			return;

		BasicString tmp;
		tmp.Reserve(Size());

		Usize lastPos = 0;

		while (pos != NPos)
		{
			tmp.PushBack(Data() + lastPos, pos - lastPos);

			if (!to.Empty())
				tmp.PushBack(to);

			lastPos = pos + from.Size(); // 跳过当前的 from 串
			pos = Find(from, lastPos);   // 继续查找下一个
		}

		if (lastPos < Size())
		{
			tmp.PushBack(Data() + lastPos, Size() - lastPos);
		}

		*this = std::move(tmp);
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
	typename BasicString<CharType>::iterator BasicString<CharType>::begin() noexcept
	{
		return Iterator(Data());
	}

	template <typename CharType>
	typename BasicString<CharType>::iterator BasicString<CharType>::end() noexcept
	{
		return Iterator(DataEnd());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_iterator BasicString<CharType>::begin() const noexcept
	{
		return const_iterator(Data());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_iterator BasicString<CharType>::end() const noexcept
	{
		return const_iterator(DataEnd());
	}

	template <typename CharType>
	typename BasicString<CharType>::reverse_iterator BasicString<CharType>::rbegin() noexcept
	{
		return ReverseIterator(end());
	}

	template <typename CharType>
	typename BasicString<CharType>::reverse_iterator BasicString<CharType>::rend() noexcept
	{
		return ReverseIterator(begin());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_reverse_iterator BasicString<CharType>::rbegin() const noexcept
	{
		return const_reverse_iterator(end());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_reverse_iterator BasicString<CharType>::rend() const noexcept
	{
		return const_reverse_iterator(begin());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_iterator BasicString<CharType>::cbegin() const noexcept
	{
		return const_iterator(Data());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_iterator BasicString<CharType>::cend() const noexcept
	{
		return const_iterator(DataEnd());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_reverse_iterator BasicString<CharType>::crbegin() noexcept
	{
		return const_reverse_iterator(cend());
	}

	template <typename CharType>
	typename BasicString<CharType>::const_reverse_iterator BasicString<CharType>::crend() noexcept
	{
		return const_reverse_iterator(cbegin());
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
	void BasicString<CharType>::ReallocateHeapBuffer(Usize capacity)
	{
		ReallocateHeapBufferByCapacity(CalculateAllocateCapacity(capacity, m_capacity, MaxStorageCapacity));
	}

	template <typename CharType>
	void BasicString<CharType>::ReallocateHeapBufferByCapacity(Usize capacity)
	{
		const CharType* oldBuffer = Data();

		CharType* newBuffer = new CharType[capacity + 1];

		// 不需要m_size > 0判断，copy会检查，并且就算为0并且触发了复制，其也不会造成副作用
		std::char_traits<CharType>::copy(newBuffer, oldBuffer, m_size);

		m_buffer.Heap = newBuffer;
		m_capacity = capacity;
		m_buffer.Heap[m_size] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::MoveToLocal()
	{
		// 这一步对m_size的检查应该在调用前进行，这里不做防御性检查
		CharType* heapBuffer = m_buffer.Heap;

		// 源头位于堆上，与局部缓冲区不重叠，可直接复制
		std::char_traits<CharType>::copy(m_buffer.Stack, heapBuffer, m_size);

		delete[] heapBuffer;

		m_capacity = LocalStorageCapacity;

		// 可以通过在copy中复制size + 1个元素来复制结尾符，但是为了规范性，仍然采用手动设置结尾符
		m_buffer.Stack[m_size] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::MoveToHeap(Usize newCapacity)
	{
		// 调用前应保证当前为本地存储；这里不做防御性检查
		const Usize capacity = CalculateAllocateCapacity(newCapacity, LocalStorageCapacity, MaxStorageCapacity);

		CharType* newBuffer = new CharType[capacity + 1];

		// 源头位于本地缓冲区(与堆不重叠)，可直接复制
		std::char_traits<CharType>::copy(newBuffer, m_buffer.Stack, m_size);
		newBuffer[m_size] = CharType();

		m_buffer.Heap = newBuffer;
		m_capacity = capacity;
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(const CharType* str, Usize size)
	{
		if (size == 0)
			return *this;

		const Usize required = m_size + size;

		if (m_capacity <= LocalStorageCapacity)
		{
			if (required > LocalStorageCapacity)
				MoveToHeap(required);
			else
			{
				std::char_traits<CharType>::copy(m_buffer.Stack + m_size, str, size);
				m_size = required;
				m_buffer.Stack[m_size] = CharType();
				return *this;
			}
		}
		else if (required > m_capacity)
			ReallocateHeapBuffer(required);

		std::char_traits<CharType>::copy(m_buffer.Heap + m_size, str, size);
		m_size = required;
		m_buffer.Heap[m_size] = CharType();
		return *this;
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(CharType ch)
	{
		return PushBack(&ch, 1);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(const CharType* str)
	{
		return PushBack(str, std::char_traits<CharType>::length(str));
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(BasicStringView<CharType> str)
	{
		return PushBack(str.Data(), str.Size());
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(std::basic_string_view<CharType> str)
	{
		return PushBack(str.data(), str.size());
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(const std::basic_string<CharType>& str)
	{
		return PushBack(str.data(), str.size());
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::PushBack(const BasicString& str)
	{
		return PushBack(str.Data(), str.m_size);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		BasicString<CharType>& BasicString<CharType>::PushBack(const Range& str)
	{
		const BasicStringView<CharType> view(str);
		return PushBack(view.Data(), view.Size());
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator+=(CharType ch)
	{
		return PushBack(ch);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator+=(const CharType* str)
	{
		return PushBack(str);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator+=(BasicStringView<CharType> str)
	{
		return PushBack(str);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator+=(std::basic_string_view<CharType> str)
	{
		return PushBack(str);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator+=(const std::basic_string<CharType>& str)
	{
		return PushBack(str);
	}

	template <typename CharType>
	BasicString<CharType>& BasicString<CharType>::operator+=(const BasicString& str)
	{
		return PushBack(str);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		BasicString<CharType>& BasicString<CharType>::operator+=(const Range& str)
	{
		return PushBack(str);
	}

	template <typename CharType>
	void BasicString<CharType>::PushFront(const BasicString& other)
	{
		PushFront(other.Data(), other.Size());
	}

	template <typename CharType>
	void BasicString<CharType>::PushFront(const CharType* str)
	{
		PushFront(str, std::char_traits<CharType>::length(str));
	}

	template <typename CharType>
	void BasicString<CharType>::PushFront(const CharType* str, Usize size)
	{
		if (str == nullptr || size == 0)
			return;

		Usize currentSize = Size();

		Reserve(currentSize + size);

		CharType* buffer = Data();

		std::copy_backward(buffer, buffer + currentSize, buffer + currentSize + size);

		std::char_traits<CharType>::copy(buffer, str, size);

		m_size += size;
		buffer[m_size] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::PushFront(CharType ch, Usize count)
	{
		if (count == 0)
			return;

		Usize size = Size();

		Reserve(size + count);

		CharType* buffer = Data();

		std::copy_backward(buffer, buffer + size, buffer + size + count);

		std::char_traits<CharType>::assign(buffer, count, ch);

		m_size += count;
		buffer[m_size] = CharType();
	}

	template <typename CharType>
	void BasicString<CharType>::PushFront(const std::basic_string<CharType>& str)
	{
		PushFront(str.data(), str.size());
	}

	template <typename CharType>
	void BasicString<CharType>::PushFront(std::basic_string_view<CharType> str)
	{
		PushFront(str.data(), str.size());
	}

	template <typename CharType>
	template <typename Range> requires (IsBasicStringRange<CharType, Range>)
		void BasicString<CharType>::PushFront(const Range& str)
	{
		PushFront(std::data(str), std::size(str));
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(const BasicString<SourceCharType>& str)
	{
		ConvertAndPushBack(str.Data(), str.Size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(BasicStringView<SourceCharType> str)
	{
		ConvertAndPushBack(str.Data(), str.Size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(const SourceCharType* str)
	{
		ConvertAndPushBack(str, std::char_traits<CharType>::length(str));
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(const SourceCharType* str, Usize size)
	{
		if constexpr (std::is_same_v<SourceCharType, CharType>)
		{
			PushBack(str, size);
			return;
		}
		else
		{
			if (str == nullptr || size == 0)
				return;

			auto begin = str;
			auto end = str + size;

			// 强异常安全保证
			BasicString result;
			result.Reserve(size);
			auto inserter = std::back_insert_iterator(result);
			while (begin != end)
			{
				boost::locale::utf::code_point c = boost::locale::utf::utf_traits<SourceCharType>::decode(begin, end);
				if (c == boost::locale::utf::illegal || c == boost::locale::utf::incomplete)
					throw BadUTFConvertException();

				boost::locale::utf::utf_traits<CharType>::encode(c, inserter);
			}

			PushBack(result);
		}
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(const std::basic_string<SourceCharType>& str)
	{
		ConvertAndPushBack(str.data(), str.size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(std::basic_string_view<SourceCharType> str)
	{
		ConvertAndPushBack(str.data(), str.size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushBack(SourceCharType ch)
	{
		ConvertAndPushBack(&ch, 1);
	}

	template <typename CharType>
	template <typename SourceCharType, typename Range> requires (IsBasicStringRange<SourceCharType, Range>)
		void BasicString<CharType>::ConvertAndPushBack(const Range& str)
	{
		ConvertAndPushBack(std::data(str), std::size(str));
	}

	template <typename CharType>
	template <typename T> requires (std::is_arithmetic_v<T> && !IsOneOf<T, char, wchar_t, char8_t, char16_t, char32_t>)
		void BasicString<CharType>::ConvertAndPushBack(T v)
	{
		char tmp[std::numeric_limits<T>::digits10 + 2];
		auto [ptr, ec] = std::to_chars(tmp, tmp + std::size(tmp), v);
		if (ec != std::error_code())
			throw BadNumberConvertException();

		Usize actualSize = ptr - tmp;
		ConvertAndPushBack(tmp, actualSize);
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushFront(const BasicString<SourceCharType>& str)
	{
		ConvertAndPushFront(str.Data(), str.Size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushFront(BasicStringView<SourceCharType> str)
	{
		ConvertAndPushFront(str.Data(), str.Size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushFront(const SourceCharType* str)
	{
		ConvertAndPushFront(str, std::char_traits<CharType>::length(str));
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushFront(const SourceCharType* str, Usize size)
	{
		if constexpr (std::is_same_v<SourceCharType, CharType>)
		{
			PushFront(str, size);
			return;
		}
		else
		{
			// 预先处理
			if (str == nullptr || size == 0)
				return;

			Usize requiredLength = 0;

			const SourceCharType* currentStartPosition = str;
			const SourceCharType* endPosition = str + size;

			// 计算转换后需要分配字符串长度
			while (currentStartPosition != endPosition)
			{
				const boost::locale::utf::code_point c = boost::locale::utf::utf_traits<SourceCharType>::decode(currentStartPosition, endPosition);
				if (c == boost::locale::utf::illegal || c == boost::locale::utf::incomplete)
					throw BadUTFConvertException();

				requiredLength += boost::locale::utf::utf_traits<CharType>::width(c);
			}

			// 重分配
			Reserve(Size() + requiredLength);

			// 移动原串
			CharType* buffer = Data();

			std::copy_backward(buffer, buffer + m_size, buffer + m_size + requiredLength);

			// 重定向到开头
			currentStartPosition = str;

			// 解码串
			while (currentStartPosition != endPosition)
			{
				const boost::locale::utf::code_point c = boost::locale::utf::utf_traits<SourceCharType>::decode(currentStartPosition, endPosition);
				// 这里不会有异常情况，因为如果有，计算长度时就已经抛出
				buffer = boost::locale::utf::utf_traits<CharType>::encode(c, buffer);
			}

			// 设置大小与\0结尾符位置为新的长度
			m_size += requiredLength;
			Data()[m_size] = CharType();
		}
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushFront(const std::basic_string<SourceCharType>& str)
	{
		ConvertAndPushFront(str.data(), str.size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertAndPushFront(std::basic_string_view<SourceCharType> str)
	{
		ConvertAndPushFront(str.data(), str.size());
	}

	template <typename CharType>
	template <typename SourceCharType, typename Range> requires (IsBasicStringRange<SourceCharType, Range>)
		void BasicString<CharType>::ConvertAndPushFront(const Range& str)
	{
		ConvertAndPushFront(std::data(str), std::size(str));
	}

	template <typename CharType>
	template <typename T> requires (std::is_arithmetic_v<T> && !IsOneOf<T, char, wchar_t, char8_t, char16_t, char32_t>)
		void BasicString<CharType>::ConvertAndPushFront(T v)
	{
		char tmp[std::numeric_limits<T>::digits10 + 2];
		auto [ptr, ec] = std::to_chars(tmp, tmp + std::size(tmp), v);
		if (ec != std::error_code())
			throw BadNumberConvertException();

		Usize actualSize = ptr - tmp;
		ConvertAndPushFront(tmp, actualSize);
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertFrom(const BasicString<SourceCharType>& str)
	{
		ConvertFrom(str.Data(), str.Size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertFrom(BasicStringView<SourceCharType> str)
	{
		ConvertFrom(str.Data(), str.Size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertFrom(const SourceCharType* str)
	{
		ConvertFrom(str, std::char_traits<CharType>::length(str));
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertFrom(const SourceCharType* str, Usize size)
	{
		Clear();
		ConvertAndPushBack(str, size);
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertFrom(const std::basic_string<SourceCharType>& str)
	{
		ConvertFrom(str.data(), str.size());
	}

	template <typename CharType>
	template <typename SourceCharType>
	void BasicString<CharType>::ConvertFrom(std::basic_string_view<SourceCharType> str)
	{
		ConvertFrom(str.data(), str.size());
	}

	template <typename CharType>
	template <typename SourceCharType, typename Range> requires (IsBasicStringRange<SourceCharType, Range>)
		void BasicString<CharType>::ConvertFrom(const Range& str)
	{
		ConvertFrom(std::data(str), std::size(str));
	}

	template <typename CharType>
	template <typename T> requires (std::is_arithmetic_v<T> && !IsOneOf<T, char, wchar_t, char8_t, char16_t, char32_t>)
		void BasicString<CharType>::ConvertFrom(T v)
	{
		char tmp[std::numeric_limits<T>::digits10 + 2];
		auto [ptr, ec] = std::to_chars(tmp, tmp + std::size(tmp), v);
		if (ec != std::error_code())
			throw BadNumberConvertException();

		Usize actualSize = ptr - tmp;
		ConvertFrom(tmp, actualSize);
	}

	template <typename CharType>
	void BasicString<CharType>::DeallocateAndRebuild(const CharType* str, Usize len)
	{
		DeallocateBuffer();

		if (len <= LocalStorageCapacity)
		{
			InitLocalBuffer();
			std::char_traits<CharType>::copy(m_buffer.Stack, str, len);
			m_size = len;
			m_buffer.Stack[m_size] = CharType();
		}
		else
		{
			InitHeapBuffer(len);
			std::char_traits<CharType>::copy(m_buffer.Heap, str, len);
			m_size = len;
			m_buffer.Heap[m_size] = CharType();
		}
	}

	template <typename CharType>
	void BasicString<CharType>::DeallocateAndRebuild(CharType ch, Usize count)
	{
		DeallocateBuffer();

		if (count <= LocalStorageCapacity)
		{
			InitLocalBuffer();
			std::char_traits<CharType>::assign(m_buffer.Stack, count, ch);
			m_size = count;
			m_buffer.Stack[m_size] = CharType();
		}
		else
		{
			InitHeapBuffer(count);
			std::char_traits<CharType>::assign(m_buffer.Heap, count, ch);
			m_size = count;
			m_buffer.Heap[m_size] = CharType();
		}
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::StartWith(CharType ch) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(ch);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::StartWith(const CharType* str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::StartWith(BasicStringView<CharType> str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::StartWith(std::basic_string_view<CharType> str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::StartWith(const std::basic_string<CharType>& str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::StartWith(const BasicString& str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(BasicStringView<CharType>(str.Data(), str.m_size));
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr bool BasicString<CharType>::StartWith(const Range& str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).StartWith(BasicStringView<CharType>(str));
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::EndWith(CharType ch) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(ch);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::EndWith(const CharType* str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::EndWith(BasicStringView<CharType> str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::EndWith(std::basic_string_view<CharType> str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::EndWith(const std::basic_string<CharType>& str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(str);
	}

	template <typename CharType>
	constexpr bool BasicString<CharType>::EndWith(const BasicString& str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(BasicStringView<CharType>(str.Data(), str.m_size));
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr bool BasicString<CharType>::EndWith(const Range& str) const noexcept
	{
		return BasicStringView<CharType>(Data(), m_size).EndWith(BasicStringView<CharType>(str));
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(CharType ch, Usize pos) const noexcept
	{
		return Find(&ch, 1, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(const CharType* str, Usize pos) const noexcept
	{
		return Find(str, std::char_traits<CharType>::length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(BasicStringView<CharType> str, Usize pos) const noexcept
	{
		return Find(str.Data(), str.Size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return Find(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return Find(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(const BasicString& str, Usize pos) const noexcept
	{
		return Find(str.Data(), str.m_size, pos);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr Usize BasicString<CharType>::Find(const Range& str, Usize pos) const noexcept
	{
		return Find(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::Find(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFind(str, size, Data(), m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(CharType ch, Usize pos) const noexcept
	{
		return RFind(&ch, 1, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(const CharType* str, Usize pos) const noexcept
	{
		return RFind(str, std::char_traits<CharType>::length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(BasicStringView<CharType> str, Usize pos) const noexcept
	{
		return RFind(str.Data(), str.Size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return RFind(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return RFind(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(const BasicString& str, Usize pos) const noexcept
	{
		return RFind(str.Data(), str.m_size, pos);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr Usize BasicString<CharType>::RFind(const Range& str, Usize pos) const noexcept
	{
		return RFind(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::RFind(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrRFind(str, size, Data(), m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(CharType ch, Usize pos) const noexcept
	{
		return FindFirstOf(&ch, 1, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(const CharType* str, Usize pos) const noexcept
	{
		return FindFirstOf(str, std::char_traits<CharType>::length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(BasicStringView<CharType> str, Usize pos) const noexcept
	{
		return FindFirstOf(str.Data(), str.Size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindFirstOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindFirstOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(const BasicString& str, Usize pos) const noexcept
	{
		return FindFirstOf(str.Data(), str.m_size, pos);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr Usize BasicString<CharType>::FindFirstOf(const Range& str, Usize pos) const noexcept
	{
		return FindFirstOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFindFirstOf(str, size, Data(), m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(CharType ch, Usize pos) const noexcept
	{
		return FindFirstNotOf(&ch, 1, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(const CharType* str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str, std::char_traits<CharType>::length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(BasicStringView<CharType> str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.Data(), str.Size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(const BasicString& str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.Data(), str.m_size, pos);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr Usize BasicString<CharType>::FindFirstNotOf(const Range& str, Usize pos) const noexcept
	{
		return FindFirstNotOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindFirstNotOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFindFirstNotOf(str, size, Data(), m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(CharType ch, Usize pos) const noexcept
	{
		return FindLastOf(&ch, 1, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(const CharType* str, Usize pos) const noexcept
	{
		return FindLastOf(str, std::char_traits<CharType>::length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(BasicStringView<CharType> str, Usize pos) const noexcept
	{
		return FindLastOf(str.Data(), str.Size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindLastOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindLastOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(const BasicString& str, Usize pos) const noexcept
	{
		return FindLastOf(str.Data(), str.m_size, pos);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr Usize BasicString<CharType>::FindLastOf(const Range& str, Usize pos) const noexcept
	{
		return FindLastOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFindLastOf(str, size, Data(), m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(CharType ch, Usize pos) const noexcept
	{
		return FindLastNotOf(&ch, 1, pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(const CharType* str, Usize pos) const noexcept
	{
		return FindLastNotOf(str, std::char_traits<CharType>::length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(BasicStringView<CharType> str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.Data(), str.Size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(const BasicString& str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.Data(), str.m_size, pos);
	}

	template <typename CharType>
	template <typename Range> requires(IsBasicStringRange<CharType, Range>)
		constexpr Usize BasicString<CharType>::FindLastNotOf(const Range& str, Usize pos) const noexcept
	{
		return FindLastNotOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	template <typename CharType>
	constexpr Usize BasicString<CharType>::FindLastNotOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFindLastNotOf(str, size, Data(), m_size, pos);

	}
	using String = BasicString<char>;
	using WString = BasicString<wchar_t>;
	using U32String = BasicString<char32_t>;
}
