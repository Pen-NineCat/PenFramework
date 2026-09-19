// File /Native/Engine/Utils/ByteArray.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../DebugTools/DebugVerify.hpp"
#include <algorithm>
#include <compare>
#include <concepts>
#include <cstring>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace PenEngine
{
	namespace ByteCompareDetail
	{
		[[nodiscard]] inline std::strong_ordering Compare(const U8* lhs, Usize lhsSize,
			const U8* rhs, Usize rhsSize) noexcept
		{
			const Usize common = lhsSize < rhsSize ? lhsSize : rhsSize;

			if (common != 0)
			{
				if (const int result = std::memcmp(lhs, rhs, common); result != 0)
					return result < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
			}

			if (lhsSize < rhsSize)
				return std::strong_ordering::less;
			if (lhsSize > rhsSize)
				return std::strong_ordering::greater;
			return std::strong_ordering::equal;
		}
	}

	// 字节容器：分配策略与 BasicString 保持一致（小容量内联 + 堆缓冲区、按位掩码与 1.5 倍增长）
	class ByteArray
	{
	public:
		constexpr static Usize LocalStorageCapacity = 16;
		constexpr static Usize AllocateMask = 15;
		constexpr static Usize MaxStorageCapacity = std::numeric_limits<Usize>::max() >> 1;
		constexpr static Usize NPos = static_cast<Usize>(-1);

		template <bool IsConst>
		class ByteIterator
		{
		public:
			using iterator_category = std::contiguous_iterator_tag;
			using iterator_concept = std::contiguous_iterator_tag;

			using value_type = U8;
			using difference_type = PtrDiff;

			using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;
			using reference = std::conditional_t<IsConst, const value_type&, value_type&>;

			constexpr ByteIterator() noexcept = default;
			constexpr explicit ByteIterator(pointer ptr) noexcept : m_ptr(ptr) {}

			// 提供 iterator 到 const_iterator 的隐式转换
			template <bool OtherConst> requires(!OtherConst && IsConst)
				/* implicit */ constexpr ByteIterator(const ByteIterator<OtherConst>& other) noexcept
				: m_ptr(other.m_ptr)
			{}

			constexpr reference operator*() const noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized byte iterator");
				return *m_ptr;
			}

			constexpr pointer operator->() const noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized byte iterator");
				return m_ptr;
			}

			constexpr ByteIterator& operator++() noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot increment value-initialized byte iterator");
				++m_ptr;
				return *this;
			}

			constexpr ByteIterator operator++(int) noexcept
			{
				ByteIterator tmp = *this;
				++(*this);
				return tmp;
			}

			constexpr ByteIterator& operator--() noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot decrement value-initialized byte iterator");
				--m_ptr;
				return *this;
			}

			constexpr ByteIterator operator--(int) noexcept
			{
				ByteIterator tmp = *this;
				--(*this);
				return tmp;
			}

			constexpr ByteIterator& operator+=(difference_type off) noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot advance value-initialized byte iterator");
				m_ptr += off;
				return *this;
			}

			constexpr ByteIterator& operator-=(difference_type off) noexcept
			{
				return (*this) += -off;
			}

			[[nodiscard]] constexpr friend ByteIterator operator+(ByteIterator it, difference_type off) noexcept
			{
				it += off;
				return it;
			}

			[[nodiscard]] constexpr friend ByteIterator operator+(difference_type off, ByteIterator it) noexcept
			{
				it += off;
				return it;
			}

			[[nodiscard]] constexpr ByteIterator operator-(difference_type off) const noexcept
			{
				ByteIterator tmp = *this;
				tmp -= off;
				return tmp;
			}

			constexpr difference_type operator-(const ByteIterator& other) const noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr && other.m_ptr, "cannot subtract value-initialized byte iterator");
				return static_cast<difference_type>(m_ptr - other.m_ptr);
			}

			constexpr reference operator[](difference_type off) const noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized byte iterator");
				return m_ptr[off];
			}

			constexpr bool operator==(const ByteIterator&) const noexcept = default;
			constexpr auto operator<=>(const ByteIterator&) const noexcept = default;

			[[nodiscard]] constexpr pointer Data() const noexcept
			{
				DEBUG_VERIFY_REPORT(m_ptr, "cannot dereference value-initialized byte iterator");
				return m_ptr;
			}
		private:
			template <bool> friend class ByteIterator;

			pointer m_ptr = nullptr;
		};

		using Iterator = ByteIterator<false>;
		using ConstIterator = ByteIterator<true>;
		using ReverseIterator = std::reverse_iterator<Iterator>;
		using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

		ByteArray() noexcept;
		explicit ByteArray(Usize initialCapacity);

		ByteArray(const ByteArray& other);
		ByteArray(ByteArray&& other) noexcept;
		ByteArray& operator=(const ByteArray& other);
		ByteArray& operator=(ByteArray&& other) noexcept;

		// copy 为 false 时直接接管 buffer 的所有权（要求由 new U8[] 分配，且不再由调用方释放）
		explicit ByteArray(const void* data, Usize size, bool copy = true);
		explicit ByteArray(const std::vector<U8>& data);
		template <typename Range> requires std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, U8>
			&& (!std::same_as<std::remove_cvref_t<Range>, std::vector<U8>>)
			explicit ByteArray(const Range& data);

		~ByteArray();

		ByteArray& operator=(const std::vector<U8>& data);

		[[nodiscard]] U8& operator[](Usize pos) noexcept;
		[[nodiscard]] const U8& operator[](Usize pos) const noexcept;

		ByteArray& Append(const void* data, Usize size);
		ByteArray& Append(const std::vector<U8>& data);
		ByteArray& Append(const ByteArray& other);

		[[nodiscard]] U8* Data() noexcept;
		[[nodiscard]] const U8* Data() const noexcept;
		[[nodiscard]] U8* DataEnd() noexcept;
		[[nodiscard]] const U8* DataEnd() const noexcept;

		[[nodiscard]] Usize Size() const noexcept;
		[[nodiscard]] Usize Capacity() const noexcept;

		[[nodiscard]] bool Empty() const noexcept;

		void Clear() noexcept;

		void Resize(Usize size);
		void Reserve(Usize capacity);

		/// @brief 将逻辑长度直接设为size，但不零填充新增区间
		/// @note 专供"紧接着会覆盖整段内容"的读取场景（例：作为IO读入缓冲区），
		///       可避免Resize对大缓冲区做一次多余的memset
		void DiscardForRead(Usize size);

		void ShrinkToFit();
		void RequestToFit();

		[[nodiscard]] std::span<const U8> Subspan(Usize pos, Usize count = NPos) const noexcept;

		// 以下函数或using声明为专为STL算法提供的小写版本
		using value_type = U8;
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
		[[nodiscard]] bool empty() const noexcept;

		[[nodiscard]] reference front() noexcept;
		[[nodiscard]] const_reference front() const noexcept;
		[[nodiscard]] reference back() noexcept;
		[[nodiscard]] const_reference back() const noexcept;

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

		[[nodiscard]] const_reverse_iterator crbegin() const noexcept;
		[[nodiscard]] const_reverse_iterator crend() const noexcept;

		[[nodiscard]] iterator find(value_type value) noexcept;
		[[nodiscard]] const_iterator find(value_type value) const noexcept;

		void push_back(value_type value) { PushBack(value); }
		void pop_back() noexcept;

		ByteArray& PushBack(value_type value);
		ByteArray& operator+=(value_type value);
		ByteArray& operator+=(const ByteArray& other);

		[[nodiscard]] friend ByteArray operator+(const ByteArray& lhs, const ByteArray& rhs)
		{
			ByteArray result(lhs.Size() + rhs.Size());
			result.Append(lhs);
			result.Append(rhs);
			return result;
		}

		[[nodiscard]] friend ByteArray operator+(const ByteArray& lhs, value_type value)
		{
			ByteArray result(lhs.Size() + 1);
			result.Append(lhs);
			result.PushBack(value);
			return result;
		}

		[[nodiscard]] friend ByteArray operator+(value_type value, const ByteArray& rhs)
		{
			ByteArray result(rhs.Size() + 1);
			result.PushBack(value);
			result.Append(rhs);
			return result;
		}

		[[nodiscard]] friend std::strong_ordering operator<=>(const ByteArray& lhs, const ByteArray& rhs) noexcept
		{
			return ByteCompareDetail::Compare(lhs.Data(), lhs.Size(), rhs.Data(), rhs.Size());
		}

		[[nodiscard]] friend std::strong_ordering operator<=>(const ByteArray& lhs,
			const std::vector<U8>& rhs) noexcept
		{
			return ByteCompareDetail::Compare(lhs.Data(), lhs.Size(), rhs.data(), rhs.size());
		}

		[[nodiscard]] friend std::strong_ordering operator<=>(const std::vector<U8>& lhs,
			const ByteArray& rhs) noexcept
		{
			return ByteCompareDetail::Compare(lhs.data(), lhs.size(), rhs.Data(), rhs.Size());
		}

		template <typename Range> requires std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, U8>
			&& (!std::same_as<std::remove_cvref_t<Range>, std::vector<U8>>)
			[[nodiscard]] friend std::strong_ordering operator<=>(const ByteArray& lhs, const Range& rhs) noexcept
		{
			return ByteCompareDetail::Compare(lhs.Data(), lhs.Size(), std::data(rhs), std::size(rhs));
		}

		template <typename Range> requires std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, U8>
			&& (!std::same_as<std::remove_cvref_t<Range>, std::vector<U8>>)
			[[nodiscard]] friend std::strong_ordering operator<=>(const Range& lhs, const ByteArray& rhs) noexcept
		{
			return ByteCompareDetail::Compare(std::data(lhs), std::size(lhs), rhs.Data(), rhs.Size());
		}

		[[nodiscard]] friend bool operator==(const ByteArray& lhs, const ByteArray& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend bool operator==(const ByteArray& lhs, const std::vector<U8>& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		[[nodiscard]] friend bool operator==(const std::vector<U8>& lhs, const ByteArray& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		template <typename Range> requires std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, U8>
			&& (!std::same_as<std::remove_cvref_t<Range>, std::vector<U8>>)
			[[nodiscard]] friend bool operator==(const ByteArray& lhs, const Range& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}

		template <typename Range> requires std::ranges::contiguous_range<Range>
			&& std::ranges::sized_range<Range>
			&& std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, U8>
			&& (!std::same_as<std::remove_cvref_t<Range>, std::vector<U8>>)
			[[nodiscard]] friend bool operator==(const Range& lhs, const ByteArray& rhs) noexcept
		{
			return (lhs <=> rhs) == std::strong_ordering::equal;
		}
	private:
		static Usize CalculateAllocateCapacity(Usize requestCapacity, Usize currentCapacity) noexcept;

		void DeallocateBuffer() noexcept;

		void InitLocalBuffer() noexcept;
		void InitHeapBuffer(Usize initialCapacity);

		void ReallocateHeapBuffer(Usize capacity);
		void ReallocateHeapBufferByCapacity(Usize capacity);

		void MoveToHeap(Usize newCapacity);
		void MoveToLocal();

		union ByteBuffer
		{
			// 多出的一字节用于存放结尾哨兵：与容量判定无关，且避免写越界到 m_capacity
			U8 Stack[LocalStorageCapacity + 1];
			U8* Heap;
		};

		ByteBuffer m_buffer;
		Usize m_capacity = 0;
		Usize m_size = 0;
	};

	inline ByteArray::ByteArray() noexcept
	{
		InitLocalBuffer();
	}

	inline ByteArray::ByteArray(Usize initialCapacity)
	{
		if (initialCapacity <= LocalStorageCapacity)
			InitLocalBuffer();
		else
			InitHeapBuffer(initialCapacity);
	}

	inline ByteArray::ByteArray(const ByteArray& other) : ByteArray(other.Data(), other.Size())
	{}

	inline ByteArray::ByteArray(ByteArray&& other) noexcept
	{
		InitLocalBuffer();
		std::swap(m_buffer, other.m_buffer);
		std::swap(m_capacity, other.m_capacity);
		std::swap(m_size, other.m_size);
	}

	inline ByteArray& ByteArray::operator=(const ByteArray& other)
	{
		if (&other == this)
			return *this;

		Clear();
		Append(other);
		return *this;
	}

	inline ByteArray& ByteArray::operator=(ByteArray&& other) noexcept
	{
		if (&other == this)
			return *this;

		DeallocateBuffer();
		std::swap(m_buffer, other.m_buffer);
		std::swap(m_capacity, other.m_capacity);
		std::swap(m_size, other.m_size);
		return *this;
	}

	inline ByteArray::ByteArray(const void* data, Usize size, bool copy)
	{
		if (!copy)
		{
			// 接管外部缓冲区：容量即长度，Data() 依赖容量落在本地阈值之外来选中堆指针分支
			DEBUG_VERIFY_REPORT(size > LocalStorageCapacity, "Adopted buffer should be larger than local storage");
			DEBUG_VERIFY_REPORT(data != nullptr, "Invalid Pointer Argument");

			m_capacity = size;
			m_size = size;
			m_buffer.Heap = static_cast<U8*>(const_cast<void*>(data));
			return;
		}

		if (size <= LocalStorageCapacity)
		{
			InitLocalBuffer();

			if (size != 0)
			{
				std::memcpy(m_buffer.Stack, data, size);
				m_size = size;
				m_buffer.Stack[m_size] = U8();
			}
			return;
		}

		InitHeapBuffer(size);
		std::memcpy(m_buffer.Heap, data, size);
		m_size = size;
		m_buffer.Heap[m_size] = U8();
	}

	inline ByteArray::ByteArray(const std::vector<U8>& data) : ByteArray(data.data(), data.size())
	{}

	template <typename Range> requires std::ranges::contiguous_range<Range>
		&& std::ranges::sized_range<Range>
		&& std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, U8>
		&& (!std::same_as<std::remove_cvref_t<Range>, std::vector<U8>>)
		inline ByteArray::ByteArray(const Range& data) : ByteArray(std::data(data), std::size(data))
	{}

	inline ByteArray::~ByteArray()
	{
		DeallocateBuffer();
	}

	inline ByteArray& ByteArray::operator=(const std::vector<U8>& data)
	{
		Clear();
		Append(data);
		return *this;
	}

	inline U8& ByteArray::operator[](Usize pos) noexcept
	{
		DEBUG_VERIFY_REPORT(pos < m_size, "Invalid Argument");
		return Data()[pos];
	}

	inline const U8& ByteArray::operator[](Usize pos) const noexcept
	{
		DEBUG_VERIFY_REPORT(pos < m_size, "Invalid Argument");
		return Data()[pos];
	}

	inline ByteArray& ByteArray::Append(const void* data, Usize size)
	{
		if (data == nullptr || size == 0)
			return *this;

		const Usize required = m_size + size;

		DEBUG_VERIFY_REPORT(required >= m_size, "ByteArray append overflow");
		DEBUG_VERIFY_REPORT(required <= MaxStorageCapacity, "ByteArray exceed max storage capacity");

		// 自追加时源区间落在自身缓冲区中，扩容会使指针失效，因此先记录偏移量
		const auto source = static_cast<const U8*>(data);
		PtrDiff selfOffset = -1;

		if (source >= Data() && source <= DataEnd())
		{
			selfOffset = static_cast<PtrDiff>(source - Data());
			DEBUG_VERIFY_REPORT(selfOffset + static_cast<PtrDiff>(size) <= static_cast<PtrDiff>(m_size),
				"ByteArray append source range out of range");
		}

		if (m_capacity <= LocalStorageCapacity)
		{
			if (required > LocalStorageCapacity)
				MoveToHeap(required);
			else
			{
				// memmove 允许源与目标区间重叠
				std::memmove(m_buffer.Stack + m_size, data, size);
				m_size = required;
				m_buffer.Stack[m_size] = U8();
				return *this;
			}
		}
		else if (required > m_capacity)
			ReallocateHeapBuffer(required);

		if (selfOffset >= 0)
			data = Data() + selfOffset;

		std::memmove(m_buffer.Heap + m_size, data, size);
		m_size = required;
		m_buffer.Heap[m_size] = U8();
		return *this;
	}

	inline ByteArray& ByteArray::Append(const std::vector<U8>& data)
	{
		return Append(data.data(), data.size());
	}

	inline ByteArray& ByteArray::Append(const ByteArray& other)
	{
		return Append(other.Data(), other.Size());
	}

	inline U8* ByteArray::Data() noexcept
	{
		return m_capacity <= LocalStorageCapacity ? m_buffer.Stack : m_buffer.Heap;
	}

	inline const U8* ByteArray::Data() const noexcept
	{
		return m_capacity <= LocalStorageCapacity ? m_buffer.Stack : m_buffer.Heap;
	}

	inline U8* ByteArray::DataEnd() noexcept
	{
		return Data() + m_size;
	}

	inline const U8* ByteArray::DataEnd() const noexcept
	{
		return Data() + m_size;
	}

	inline Usize ByteArray::Size() const noexcept
	{
		return m_size;
	}

	inline Usize ByteArray::Capacity() const noexcept
	{
		return m_capacity;
	}

	inline bool ByteArray::Empty() const noexcept
	{
		return m_size == 0;
	}

	inline void ByteArray::Clear() noexcept
	{
		m_size = 0;
		Data()[0] = U8();
	}

	inline void ByteArray::Resize(Usize size)
	{
		if (size > m_capacity)
			Reserve(size);

		U8* buffer = Data();

		if (size > m_size)
			std::memset(buffer + m_size, 0, size - m_size);

		m_size = size;
		buffer[m_size] = U8();
	}

	inline void ByteArray::Reserve(Usize capacity)
	{
		if (capacity <= m_capacity)
			return;

		if (m_capacity > LocalStorageCapacity)
			ReallocateHeapBuffer(capacity);
		else
			MoveToHeap(capacity);
	}

	inline void ByteArray::DiscardForRead(Usize size)
	{
		if (size > m_capacity)
			Reserve(size);

		m_size = size;
		Data()[m_size] = U8();
	}

	inline void ByteArray::ShrinkToFit()
	{
		// 与 BasicString 保持一致的宽容策略：浪费不足一半且不超过512字节时不收缩
		if (m_capacity > LocalStorageCapacity)
		{
			const Usize waste = Capacity() - Size();
			if (waste <= Size() * 1.5 && waste <= 512)
				return;

			// 留出 1.25 倍成长余量；Size() * 5 / 4 对 Size() < 4 恒不大于本地阈值，因此不会退化为非法堆容量
			const Usize actualFitCapacity = Size() * 5 / 4;

			if (actualFitCapacity <= LocalStorageCapacity)
				MoveToLocal();
			else
				ReallocateHeapBufferByCapacity(actualFitCapacity);
		}
	}

	inline void ByteArray::RequestToFit()
	{
		if (m_capacity <= LocalStorageCapacity)
			return;

		if (m_size <= LocalStorageCapacity)
			MoveToLocal();
		else if (m_size < m_capacity)
			ReallocateHeapBufferByCapacity(m_size);
	}

	inline std::span<const U8> ByteArray::Subspan(Usize pos, Usize count) const noexcept
	{
		DEBUG_VERIFY_REPORT(pos <= m_size, "ByteArray subspan position out of range");
		return std::span<const U8>(Data() + pos, std::min(count, m_size - pos));
	}

	inline ByteArray::pointer ByteArray::data() noexcept
	{
		return Data();
	}

	inline ByteArray::const_pointer ByteArray::data() const noexcept
	{
		return Data();
	}

	inline size_t ByteArray::size() const noexcept
	{
		return Size();
	}

	inline size_t ByteArray::capacity() const noexcept
	{
		return Capacity();
	}

	inline bool ByteArray::empty() const noexcept
	{
		return Empty();
	}

	inline ByteArray::reference ByteArray::front() noexcept
	{
		DEBUG_VERIFY_REPORT(m_size != 0, "ByteArray is empty");
		return Data()[0];
	}

	inline ByteArray::const_reference ByteArray::front() const noexcept
	{
		DEBUG_VERIFY_REPORT(m_size != 0, "ByteArray is empty");
		return Data()[0];
	}

	inline ByteArray::reference ByteArray::back() noexcept
	{
		DEBUG_VERIFY_REPORT(m_size != 0, "ByteArray is empty");
		return Data()[m_size - 1];
	}

	inline ByteArray::const_reference ByteArray::back() const noexcept
	{
		DEBUG_VERIFY_REPORT(m_size != 0, "ByteArray is empty");
		return Data()[m_size - 1];
	}

	inline ByteArray::iterator ByteArray::begin() noexcept
	{
		return Iterator(Data());
	}

	inline ByteArray::iterator ByteArray::end() noexcept
	{
		return Iterator(Data() + m_size);
	}

	inline ByteArray::const_iterator ByteArray::begin() const noexcept
	{
		return const_iterator(Data());
	}

	inline ByteArray::const_iterator ByteArray::end() const noexcept
	{
		return const_iterator(Data() + m_size);
	}

	inline ByteArray::reverse_iterator ByteArray::rbegin() noexcept
	{
		return ReverseIterator(end());
	}

	inline ByteArray::reverse_iterator ByteArray::rend() noexcept
	{
		return ReverseIterator(begin());
	}

	inline ByteArray::const_reverse_iterator ByteArray::rbegin() const noexcept
	{
		return const_reverse_iterator(end());
	}

	inline ByteArray::const_reverse_iterator ByteArray::rend() const noexcept
	{
		return const_reverse_iterator(begin());
	}

	inline ByteArray::const_iterator ByteArray::cbegin() const noexcept
	{
		return const_iterator(Data());
	}

	inline ByteArray::const_iterator ByteArray::cend() const noexcept
	{
		return const_iterator(Data() + m_size);
	}

	inline ByteArray::const_reverse_iterator ByteArray::crbegin() const noexcept
	{
		return const_reverse_iterator(cend());
	}

	inline ByteArray::const_reverse_iterator ByteArray::crend() const noexcept
	{
		return const_reverse_iterator(cbegin());
	}

	inline ByteArray::iterator ByteArray::find(value_type value) noexcept
	{
		return std::find(begin(), end(), value);
	}

	inline ByteArray::const_iterator ByteArray::find(value_type value) const noexcept
	{
		return std::find(begin(), end(), value);
	}

	inline void ByteArray::pop_back() noexcept
	{
		DEBUG_VERIFY_REPORT(m_size != 0, "ByteArray is empty");

		if (m_size != 0)
		{
			--m_size;
			Data()[m_size] = U8();
		}
	}

	inline ByteArray& ByteArray::PushBack(value_type value)
	{
		return Append(&value, 1);
	}

	inline ByteArray& ByteArray::operator+=(value_type value)
	{
		return PushBack(value);
	}

	inline ByteArray& ByteArray::operator+=(const ByteArray& other)
	{
		return Append(other);
	}

	inline Usize ByteArray::CalculateAllocateCapacity(Usize requestCapacity, Usize currentCapacity) noexcept
	{
		const Usize masked = requestCapacity | AllocateMask;
		if (masked > MaxStorageCapacity)
			return MaxStorageCapacity;

		if (currentCapacity > MaxStorageCapacity - currentCapacity / 2)
			return MaxStorageCapacity;

		return std::max(masked, currentCapacity + currentCapacity / 2);
	}

	inline void ByteArray::DeallocateBuffer() noexcept
	{
		if (m_capacity > LocalStorageCapacity)
			delete[] m_buffer.Heap;
		InitLocalBuffer();
	}

	inline void ByteArray::InitLocalBuffer() noexcept
	{
		m_capacity = LocalStorageCapacity;
		m_size = 0;
		m_buffer.Stack[0] = U8();
	}

	inline void ByteArray::InitHeapBuffer(Usize initialCapacity)
	{
		initialCapacity = CalculateAllocateCapacity(initialCapacity, LocalStorageCapacity);

		U8* buffer = new U8[initialCapacity + 1];

		m_size = 0;
		m_capacity = initialCapacity;
		m_buffer.Heap = buffer;
		m_buffer.Heap[0] = U8();
	}

	inline void ByteArray::ReallocateHeapBuffer(Usize capacity)
	{
		ReallocateHeapBufferByCapacity(CalculateAllocateCapacity(capacity, m_capacity));
	}

	inline void ByteArray::ReallocateHeapBufferByCapacity(Usize capacity)
	{
		// 容量不得回落到本地阈值，否则 Data() 会切换到本地缓冲区而泄漏堆内存
		DEBUG_VERIFY_REPORT(capacity > LocalStorageCapacity, "Heap capacity should be larger than local storage");

		const U8* oldBuffer = Data();

		U8* newBuffer = new U8[capacity + 1];

		if (m_size != 0)
			std::memcpy(newBuffer, oldBuffer, m_size);

		delete[] oldBuffer;

		m_capacity = capacity;
		m_buffer.Heap = newBuffer;
		m_buffer.Heap[m_size] = U8();
	}

	inline void ByteArray::MoveToLocal()
	{
		// 这一步对m_size的检查应该在调用前进行，这里不做防御性检查
		U8* heapBuffer = m_buffer.Heap;

		// 源头位于堆上，与局部缓冲区不重叠，可直接复制
		if (m_size != 0)
			std::memcpy(m_buffer.Stack, heapBuffer, m_size);

		delete[] heapBuffer;

		m_capacity = LocalStorageCapacity;
		m_buffer.Stack[m_size] = U8();
	}

	inline void ByteArray::MoveToHeap(Usize newCapacity)
	{
		// 调用前应保证当前为本地存储；这里不做防御性检查
		const Usize capacity = CalculateAllocateCapacity(newCapacity, LocalStorageCapacity);

		U8* newBuffer = new U8[capacity + 1];

		// 源头位于本地缓冲区(与堆不重叠)，可直接复制
		if (m_size != 0)
			std::memcpy(newBuffer, m_buffer.Stack, m_size);

		newBuffer[m_size] = U8();

		m_capacity = capacity;
		m_buffer.Heap = newBuffer;
	}
}
