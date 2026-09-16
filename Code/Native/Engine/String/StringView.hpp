// File /Native/Engine/String/StringView.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../DebugTools/DebugVerify.hpp"
#include "StrSearchUtils.hpp"
#include <compare>
#include <format>
#include <iterator>
#include <string>
#include <string_view>

namespace PenEngine
{
	namespace StringCompareDetail
	{
		template <typename CharType>
		[[nodiscard]] constexpr std::strong_ordering Compare(const CharType* lhs, Usize lhsSize,
			const CharType* rhs, Usize rhsSize) noexcept
		{
			const Usize common = lhsSize < rhsSize ? lhsSize : rhsSize;
			const int result = StringSearchDetail::CompareChars(lhs, rhs, common);
			if (result < 0)
				return std::strong_ordering::less;
			if (result > 0)
				return std::strong_ordering::greater;
			if (lhsSize < rhsSize)
				return std::strong_ordering::less;
			if (lhsSize > rhsSize)
				return std::strong_ordering::greater;
			return std::strong_ordering::equal;
		}

		template <typename CharType, typename Range>
		concept IsCompareRange = std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::ranges::range_value_t<Range>, CharType>
			&& !std::same_as<std::remove_cvref_t<Range>, std::basic_string<CharType>>
			&& !std::same_as<std::remove_cvref_t<Range>, std::basic_string_view<CharType>>;

		// 查找用的模板 rng 约束：具名重载已覆盖的类型不参与，避免重载歧义
		template <typename CharType>
		class BasicStringView;

		template <typename CharType, typename Range>
		concept IsSearchRange = std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::ranges::range_value_t<Range>, CharType>
			&& !std::same_as<std::remove_cvref_t<Range>, BasicStringView<CharType>>
			&& !std::same_as<std::remove_cvref_t<Range>, std::basic_string<CharType>>
			&& !std::same_as<std::remove_cvref_t<Range>, std::basic_string_view<CharType>>;
	}

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

	// 迭代器与 nullptr 的比较（std::string_view::iterator 亦提供该能力）
	template <typename CharType>
	[[nodiscard]] constexpr bool operator==(const StringConstIterator<CharType>& it, std::nullptr_t) noexcept
	{
		return it.Data() == nullptr;
	}

	template <typename CharType>
	[[nodiscard]] constexpr bool operator==(std::nullptr_t, const StringConstIterator<CharType>& it) noexcept
	{
		return it.Data() == nullptr;
	}

	// BasicStringView 无法包含 BasicString(会形成循环依赖)，因此
	// 凡是需要同时接受 BasicString 的位置一律使用模板 rng 重载
	template <typename CharType>
	class BasicStringView
	{
	public:
		using ReverseIterator = std::reverse_iterator<StringConstIterator<CharType>>;
		using ConstReverseIterator = std::reverse_iterator<StringConstIterator<CharType>>;
		using ConstIterator = StringConstIterator<CharType>;

		constexpr static Usize NPos = static_cast<Usize>(-1);

		constexpr BasicStringView() noexcept = default;

		/*implicit*/ constexpr BasicStringView(std::nullptr_t) = delete;
		/*implicit*/ constexpr BasicStringView(const CharType* str) noexcept : m_str(str), m_size(StringSearchDetail::Length(str)) {}
		constexpr BasicStringView(const CharType* str, Usize count) noexcept : m_str(str), m_size(count) {}
		constexpr BasicStringView(const CharType* begin, const CharType* end) noexcept : m_str(begin), m_size(end - begin) {}

		constexpr BasicStringView(ConstIterator begin, ConstIterator end) noexcept : m_str(begin.Data()), m_size(end - begin) {}

		template <typename Range> requires(!std::same_as<std::remove_cvref_t<Range>, BasicStringView>
		&& std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::ranges::range_value_t<Range>, CharType>
			&& !std::is_convertible_v<Range, const CharType*>
			&& !requires(std::remove_cvref_t<Range>& Rng)
		{
			Rng.operator BasicStringView<CharType>();
		})
			/*implicit*/ constexpr BasicStringView(Range&& rng) noexcept(noexcept(std::data(rng)) && noexcept(std::size(rng)))
			: m_str(std::data(rng)), m_size(static_cast<Usize>(std::size(rng)))
		{}

		/* implicit */ constexpr operator std::basic_string_view<CharType>() const noexcept
		{
			return std::basic_string_view<CharType>(Data(), Size());
		}

		template <typename Range> requires(!std::same_as<std::remove_cvref_t<Range>, BasicStringView>
		&& std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::ranges::range_value_t<Range>, CharType>
			&& !std::is_convertible_v<Range, const CharType*>
			&& !requires(std::remove_cvref_t<Range>& Rng)
		{
			rng.operator BasicStringView();
		})
			constexpr BasicStringView& operator=(Range&& rng) noexcept(noexcept(std::data(rng)) && noexcept(std::size(rng)))
		{
			m_str = std::data(rng);
			m_size = static_cast<Usize>(std::size(rng));
			return *this;
		}

		constexpr BasicStringView(const BasicStringView&) noexcept = default;
		constexpr BasicStringView(BasicStringView&&) noexcept = default;
		constexpr BasicStringView& operator=(const BasicStringView&) noexcept = default;
		constexpr BasicStringView& operator=(BasicStringView&&) noexcept = default;

		constexpr ~BasicStringView() noexcept = default;

		constexpr CharType operator[](Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize Capacity() const noexcept { return m_size; }
		[[nodiscard]] constexpr Usize Size() const noexcept { return m_size; }
		[[nodiscard]] constexpr Usize Length() const noexcept { return m_size; }

		BasicStringView<CharType> Subview(Usize pos, Usize len = NPos) const noexcept;

		[[nodiscard]] constexpr const CharType* Data() noexcept { return m_str; }

		[[nodiscard]] constexpr const CharType* Data() const noexcept { return m_str; }
		[[nodiscard]] constexpr const CharType* DataEnd() const noexcept { return m_str + m_size; }
		[[nodiscard]] constexpr bool Empty() const noexcept { return m_size == 0; }

		[[nodiscard]] constexpr Usize capacity() const noexcept { return m_size; }
		[[nodiscard]] constexpr Usize size() const noexcept { return m_size; }
		[[nodiscard]] constexpr Usize length() const noexcept { return m_size; }
		[[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

		[[nodiscard]] constexpr const CharType* data() noexcept { return m_str; }
		[[nodiscard]] constexpr const CharType* data() const noexcept { return m_str; }

		[[nodiscard]] constexpr ConstIterator begin() const noexcept { return ConstIterator(m_str); }
		[[nodiscard]] constexpr ConstIterator end() const noexcept { return ConstIterator(m_str + m_size); }

		[[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(end()); }
		[[nodiscard]] constexpr ConstReverseIterator rend() const noexcept { return ConstReverseIterator(begin()); }

		// ================================================================
		// StartWith / EndWith
		// ================================================================

		[[nodiscard]] constexpr bool StartWith(CharType ch) const noexcept;
		[[nodiscard]] constexpr bool StartWith(const CharType* str) const noexcept;
		[[nodiscard]] constexpr bool StartWith(std::basic_string_view<CharType> str) const noexcept;
		[[nodiscard]] constexpr bool StartWith(const std::basic_string<CharType>& str) const noexcept;
		template <typename Range>
		[[nodiscard]] constexpr bool StartWith(const Range& str) const noexcept
		{
			return StartWith(BasicStringView(str));
		}

		[[nodiscard]] constexpr bool EndWith(CharType ch) const noexcept;
		[[nodiscard]] constexpr bool EndWith(const CharType* str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(std::basic_string_view<CharType> str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(const std::basic_string<CharType>& str) const noexcept;
		[[nodiscard]] constexpr bool EndWith(BasicStringView str) const noexcept;
		template <typename Range>
		[[nodiscard]] constexpr bool EndWith(const Range& str) const noexcept
		{
			return EndWith(BasicStringView(str));
		}

		// ================================================================
		// Find 家族：返回命中位置下标，未命中返回 NPos
		// 唯一的实现体是 (str, size, pos) 重载，其余重载均为其路由
		// ================================================================

		[[nodiscard]] constexpr Usize Find(CharType ch, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const CharType* str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(BasicStringView str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(std::basic_string_view<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const std::basic_string<CharType>& str, Usize pos = 0) const noexcept;
		template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
			[[nodiscard]] constexpr Usize Find(const Range& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize Find(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize RFind(CharType ch, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const CharType* str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(BasicStringView str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(std::basic_string_view<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const std::basic_string<CharType>& str, Usize pos = NPos) const noexcept;
		template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
			[[nodiscard]] constexpr Usize RFind(const Range& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize RFind(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindFirstOf(CharType ch, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const CharType* str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(BasicStringView str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(std::basic_string_view<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const std::basic_string<CharType>& str, Usize pos = 0) const noexcept;
		template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindFirstOf(const Range& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstOf(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindFirstNotOf(CharType ch, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const CharType* str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(BasicStringView str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(std::basic_string_view<CharType> str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const std::basic_string<CharType>& str, Usize pos = 0) const noexcept;
		template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindFirstNotOf(const Range& str, Usize pos = 0) const noexcept;
		[[nodiscard]] constexpr Usize FindFirstNotOf(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindLastOf(CharType ch, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const CharType* str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(BasicStringView str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(std::basic_string_view<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const std::basic_string<CharType>& str, Usize pos = NPos) const noexcept;
		template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindLastOf(const Range& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastOf(const CharType* str, Usize size, Usize pos) const noexcept;

		[[nodiscard]] constexpr Usize FindLastNotOf(CharType ch, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const CharType* str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(BasicStringView str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(std::basic_string_view<CharType> str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const std::basic_string<CharType>& str, Usize pos = NPos) const noexcept;
		template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
			[[nodiscard]] constexpr Usize FindLastNotOf(const Range& str, Usize pos = NPos) const noexcept;
		[[nodiscard]] constexpr Usize FindLastNotOf(const CharType* str, Usize size, Usize pos) const noexcept;

		// ================================================================
		// operator<=>：具名重载覆盖 const CharType* / std::basic_string /
		// std::basic_string_view / BasicStringView，模板 rng 重载支持 BasicString
		// ================================================================

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(BasicStringView lhs, BasicStringView rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.m_str, lhs.m_size, rhs.m_str, rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(BasicStringView lhs, const CharType* rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.m_str, lhs.m_size, rhs, StringSearchDetail::Length(rhs));
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const CharType* lhs, BasicStringView rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs, StringSearchDetail::Length(lhs), rhs.m_str, rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(BasicStringView lhs,
			std::basic_string_view<CharType> rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.m_str, lhs.m_size, rhs.data(), rhs.size());
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(std::basic_string_view<CharType> lhs,
			BasicStringView rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.data(), lhs.size(), rhs.m_str, rhs.m_size);
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(BasicStringView lhs,
			const std::basic_string<CharType>& rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.m_str, lhs.m_size, rhs.data(), rhs.size());
		}

		[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const std::basic_string<CharType>& lhs,
			BasicStringView rhs) noexcept
		{
			return StringCompareDetail::Compare(lhs.data(), lhs.size(), rhs.m_str, rhs.m_size);
		}

		template <typename Range> requires(StringCompareDetail::IsCompareRange<CharType, Range>)
			[[nodiscard]] friend constexpr std::strong_ordering operator<=>(BasicStringView lhs, const Range& rhs) noexcept
		{
			const BasicStringView rhsView(rhs);
			return StringCompareDetail::Compare(lhs.m_str, lhs.m_size, rhsView.m_str, rhsView.m_size);
		}

		template <typename Range> requires(StringCompareDetail::IsCompareRange<CharType, Range>)
			[[nodiscard]] friend constexpr std::strong_ordering operator<=>(const Range& lhs, BasicStringView rhs) noexcept
		{
			const BasicStringView lhsView(lhs);
			return StringCompareDetail::Compare(lhsView.m_str, lhsView.m_size, rhs.m_str, rhs.m_size);
		}

		// MSVC 不会由隐藏友元 operator<=> 重写 ==，因此显式提供 operator==，
		// != 再由 == 重写而来；<=> 仍负责 < <= > >=
		[[nodiscard]] friend constexpr bool operator==(BasicStringView lhs, BasicStringView rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(BasicStringView lhs, const CharType* rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const CharType* lhs, BasicStringView rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		// 与单个字符比较：长度必须为 1
		[[nodiscard]] friend constexpr bool operator==(BasicStringView lhs, CharType rhs) noexcept
		{
			return lhs.m_size == 1 && lhs.m_str[0] == rhs;
		}

		[[nodiscard]] friend constexpr bool operator==(CharType lhs, BasicStringView rhs) noexcept
		{
			return rhs.m_size == 1 && rhs.m_str[0] == lhs;
		}

		[[nodiscard]] friend constexpr bool operator==(BasicStringView lhs,
			std::basic_string_view<CharType> rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(std::basic_string_view<CharType> lhs,
			BasicStringView rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(BasicStringView lhs,
			const std::basic_string<CharType>& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend constexpr bool operator==(const std::basic_string<CharType>& lhs,
			BasicStringView rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		template <typename Range> requires(StringCompareDetail::IsCompareRange<CharType, Range>)
			[[nodiscard]] friend constexpr bool operator==(BasicStringView lhs, const Range& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		template <typename Range> requires(StringCompareDetail::IsCompareRange<CharType, Range>)
			[[nodiscard]] friend constexpr bool operator==(const Range& lhs, BasicStringView rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		// 对字符串类目标校验起始位置
		constexpr void VerifyPos(Usize pos) const noexcept
		{
			DEBUG_VERIFY_REPORT(pos <= m_size, "string view position out of range");
		}

		// 命中下标 -> 子视图，未命中返回空视图
		[[nodiscard]] constexpr BasicStringView SliceFrom(Usize index) const noexcept
		{
			return index == NPos ? BasicStringView() : BasicStringView(m_str + index, m_size - index);
		}
	private:
		const CharType* m_str = nullptr;
		Usize m_size = 0;
	};

	template <typename CharType>
	BasicStringView<CharType> BasicStringView<CharType>::Subview(Usize pos, Usize len) const noexcept
	{
		return BasicStringView<CharType>(Data() + pos, std::min(len, Size() - pos));
	}

	template <typename CharType>
	constexpr CharType BasicStringView<CharType>::operator[](Usize pos) const noexcept
	{
		DEBUG_VERIFY_REPORT(pos < m_size, "Invalid Argument");
		return m_str[pos];
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::StartWith(CharType ch) const noexcept
	{
		return m_size != 0 && m_str[0] == ch;
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::StartWith(std::basic_string_view<CharType> str) const noexcept
	{
		return str.size() <= m_size && StringCompareDetail::Compare(m_str, str.size(), str.data(), str.size())
			== std::strong_ordering::equal;
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::StartWith(const CharType* str) const noexcept
	{
		return StartWith(std::basic_string_view<CharType>(str));
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::StartWith(const std::basic_string<CharType>& str) const noexcept
	{
		return StartWith(std::basic_string_view<CharType>(str));
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::EndWith(CharType ch) const noexcept
	{
		return m_size != 0 && m_str[m_size - 1] == ch;
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::EndWith(std::basic_string_view<CharType> str) const noexcept
	{
		return str.size() <= m_size
			&& StringCompareDetail::Compare(m_str + (m_size - str.size()), str.size(), str.data(), str.size())
			== std::strong_ordering::equal;
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::EndWith(const CharType* str) const noexcept
	{
		return EndWith(std::basic_string_view<CharType>(str));
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::EndWith(const std::basic_string<CharType>& str) const noexcept
	{
		return EndWith(std::basic_string_view<CharType>(str));
	}

	template <typename CharType>
	constexpr bool BasicStringView<CharType>::EndWith(BasicStringView str) const noexcept
	{
		return EndWith(std::basic_string_view<CharType>(str.Data(), str.Size()));
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::Find(CharType ch, Usize pos) const noexcept
	{
		VerifyPos(pos);
		return CharFind(ch, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::Find(const CharType* str, Usize pos) const noexcept
	{
		return Find(str, StringSearchDetail::Length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::Find(BasicStringView str, Usize pos) const noexcept
	{
		return Find(str.m_str, str.m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::Find(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return Find(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::Find(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return Find(str.data(), str.size(), pos);
	}

	template <typename CharType>
	template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
		constexpr Usize BasicStringView<CharType>::Find(const Range& str, Usize pos) const noexcept
	{
		return Find(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	// 唯一实现体
	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::Find(const CharType* str, Usize size, Usize pos) const noexcept
	{
		VerifyPos(pos);
		return StrFind(str, size, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::RFind(CharType ch, Usize pos) const noexcept
	{
		return CharRFind(ch, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::RFind(const CharType* str, Usize pos) const noexcept
	{
		return RFind(str, StringSearchDetail::Length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::RFind(BasicStringView str, Usize pos) const noexcept
	{
		return RFind(str.m_str, str.m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::RFind(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return RFind(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::RFind(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return RFind(str.data(), str.size(), pos);
	}

	template <typename CharType>
	template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
		constexpr Usize BasicStringView<CharType>::RFind(const Range& str, Usize pos) const noexcept
	{
		return RFind(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	// 唯一实现体
	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::RFind(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrRFind(str, size, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstOf(CharType ch, Usize pos) const noexcept
	{
		VerifyPos(pos);
		return CharFindFirstOf(&ch, 1, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstOf(const CharType* str, Usize pos) const noexcept
	{
		return FindFirstOf(str, StringSearchDetail::Length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstOf(BasicStringView str, Usize pos) const noexcept
	{
		return FindFirstOf(str.m_str, str.m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindFirstOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindFirstOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
		constexpr Usize BasicStringView<CharType>::FindFirstOf(const Range& str, Usize pos) const noexcept
	{
		return FindFirstOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	// 唯一实现体
	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		VerifyPos(pos);
		return StrFindFirstOf(str, size, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstNotOf(CharType ch, Usize pos) const noexcept
	{
		VerifyPos(pos);
		return CharFindFirstNotOf(&ch, 1, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstNotOf(const CharType* str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str, StringSearchDetail::Length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstNotOf(BasicStringView str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.m_str, str.m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstNotOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstNotOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindFirstNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
		constexpr Usize BasicStringView<CharType>::FindFirstNotOf(const Range& str, Usize pos) const noexcept
	{
		return FindFirstNotOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	// 唯一实现体
	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindFirstNotOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		VerifyPos(pos);
		return StrFindFirstNotOf(str, size, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastOf(CharType ch, Usize pos) const noexcept
	{
		return CharFindLastOf(&ch, 1, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastOf(const CharType* str, Usize pos) const noexcept
	{
		return FindLastOf(str, StringSearchDetail::Length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastOf(BasicStringView str, Usize pos) const noexcept
	{
		return FindLastOf(str.m_str, str.m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindLastOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindLastOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
		constexpr Usize BasicStringView<CharType>::FindLastOf(const Range& str, Usize pos) const noexcept
	{
		return FindLastOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	// 唯一实现体
	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFindLastOf(str, size, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastNotOf(CharType ch, Usize pos) const noexcept
	{
		return CharFindLastNotOf(&ch, 1, m_str, m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastNotOf(const CharType* str, Usize pos) const noexcept
	{
		return FindLastNotOf(str, StringSearchDetail::Length(str), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastNotOf(BasicStringView str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.m_str, str.m_size, pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastNotOf(std::basic_string_view<CharType> str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastNotOf(const std::basic_string<CharType>& str, Usize pos) const noexcept
	{
		return FindLastNotOf(str.data(), str.size(), pos);
	}

	template <typename CharType>
	template <typename Range> requires(StringCompareDetail::IsSearchRange<CharType, Range>)
		constexpr Usize BasicStringView<CharType>::FindLastNotOf(const Range& str, Usize pos) const noexcept
	{
		return FindLastNotOf(std::data(str), static_cast<Usize>(std::size(str)), pos);
	}

	// 唯一实现体
	template <typename CharType>
	constexpr Usize BasicStringView<CharType>::FindLastNotOf(const CharType* str, Usize size, Usize pos) const noexcept
	{
		return StrFindLastNotOf(str, size, m_str, m_size, pos);
	}

	using StringView = BasicStringView<char>;
	using WStringView = BasicStringView<wchar_t>;
	using U32StringView = BasicStringView<char32_t>;
}

template <>
struct std::formatter<PenEngine::StringView, char> : std::formatter<std::string_view, char>
{
	auto format(PenEngine::StringView str, std::format_context& ctx) const
	{
		// 委托基类 format，以复用基类 parse 解析出的格式说明符
		return std::formatter<std::string_view, char>::format(std::string_view(str.Data(), str.Size()), ctx);
	}
};

template <>
struct std::formatter<PenEngine::WStringView, wchar_t> : std::formatter<std::wstring_view, wchar_t>
{
	auto format(PenEngine::WStringView str, std::wformat_context& ctx) const
	{
		return std::formatter<std::wstring_view, wchar_t>::format(std::wstring_view(str.Data(), str.Size()), ctx);
	}
};

template <typename CharType>
struct std::hash<PenEngine::BasicStringView<CharType>>
{
	static PenEngine::Usize operator()(const PenEngine::BasicStringView<CharType>& str) noexcept
	{
		return std::hash<std::basic_string_view<CharType>>::operator()(std::basic_string_view<CharType>(str.Data(), str.Size()));
	}
};
