// File /Native/Memory/Common.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: common definitions.
//
// Configuration constants, the page/size-class model, the size-class table and
// the size -> size-class mapping.  Modelled on tcmalloc's common.h, pages.h,
// sizemap.h and size_classes.cc (page-shift == 13 configuration).
//
// Deviations from tcmalloc are called out in the comments and in DESIGN.md.

#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace PenMemory
{

	// ---------------------------------------------------------------------------
	// Compiler helpers
	// ---------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#define PEN_MEMORY_PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))
#define PEN_MEMORY_PREDICT_FALSE(x) (__builtin_expect(!!(x), 0))
#define PEN_MEMORY_ALWAYS_INLINE inline __attribute__((always_inline))
#define PEN_MEMORY_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define PEN_MEMORY_PREDICT_TRUE(x) (x)
#define PEN_MEMORY_PREDICT_FALSE(x) (x)
#define PEN_MEMORY_ALWAYS_INLINE __forceinline
#define PEN_MEMORY_NOINLINE __declspec(noinline)
#else
#define PEN_MEMORY_PREDICT_TRUE(x) (x)
#define PEN_MEMORY_PREDICT_FALSE(x) (x)
#define PEN_MEMORY_ALWAYS_INLINE inline
#define PEN_MEMORY_NOINLINE
#endif

#define PEN_MEMORY_CACHELINE_SIZE 64
#if defined(_MSC_VER)
#define PEN_MEMORY_CACHELINE_ALIGNED __declspec(align(PEN_MEMORY_CACHELINE_SIZE))
#else
#define PEN_MEMORY_CACHELINE_ALIGNED alignas(PEN_MEMORY_CACHELINE_SIZE)
#endif

	// ---------------------------------------------------------------------------
	// Diagnostics.  tcmalloc uses TC_CHECK/TC_ASSERT/TC_BUG; we keep the same
	// spirit with a much smaller implementation.
	// ---------------------------------------------------------------------------
	[[noreturn]] void CrashWithMessage(const char* file, int line, const char* fmt,
		...);

#define PEN_MEMORY_BUG(fmt, ...) \
	::PenMemory::CrashWithMessage(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define PEN_MEMORY_CHECK(cond) \
	do { \
		if (PEN_MEMORY_PREDICT_FALSE(!(cond))) { \
			::PenMemory::CrashWithMessage(__FILE__, __LINE__, \
				"CHECK failed: %s", #cond); \
		} \
	} while (0)

#define PEN_MEMORY_CHECK_MSG(cond, fmt, ...) \
	do { \
		if (PEN_MEMORY_PREDICT_FALSE(!(cond))) { \
			::PenMemory::CrashWithMessage(__FILE__, __LINE__, \
				"CHECK failed: %s: " fmt, #cond, \
				##__VA_ARGS__); \
		} \
	} while (0)

#ifndef NDEBUG
#define PEN_MEMORY_ASSERT(cond) PEN_MEMORY_CHECK(cond)
#define PEN_MEMORY_ASSERT_MSG(cond, fmt, ...) PEN_MEMORY_CHECK_MSG(cond, fmt, ##__VA_ARGS__)
#else
#define PEN_MEMORY_ASSERT(cond) ((void)0)
#define PEN_MEMORY_ASSERT_MSG(cond, fmt, ...) ((void)0)
#endif

	// Used for corruption reporting (double free, wild pointer, ...).  Unlike an
	// assert this is always active: silently corrupting the heap is worse.
	[[noreturn]] void ReportCorruptedFree(const void* ptr, const char* what);

	// ---------------------------------------------------------------------------
	// Type aliases used across the module.  They match the same-named aliases in
	// the main workspace's Engine/Core/Environment.h.
	// ---------------------------------------------------------------------------
	using B8 = uint8_t;
	using U8 = uint8_t;
	using U16 = uint16_t;
	using U32 = uint32_t;
	using U64 = uint64_t;
	using I8 = int8_t;
	using I16 = int16_t;
	using I32 = int32_t;
	using I64 = int64_t;
	using Usize = size_t;
	using PtrDiff = ptrdiff_t;
	using HashID = U64;

	// ---------------------------------------------------------------------------
	// Configuration.  Mirrors tcmalloc's TCMALLOC_PAGE_SHIFT == 13 model.
	// ---------------------------------------------------------------------------
	inline constexpr Usize PageShift = 13;
	inline constexpr Usize PageSize = Usize{1} << PageShift;  // 8 KiB
	inline constexpr Usize Alignment = 8;
	inline constexpr Usize AlignmentShift = 3;  // log2(Alignment)

	// Largest allocation served by the size-class based caches (thread/central).
	inline constexpr Usize MaxSize = 256 * 1024;
	// Number of size classes, including class 0 (which means "not a size class").
	inline constexpr Usize NumClasses = 49;
	// Largest batch handed between thread and central cache (== max num_to_move).
	inline constexpr Usize MaxObjectsToMove = 32;

	// Page heap grows by at least this much.
	inline constexpr Usize MinSystemAlloc = Usize{1} << 20;  // 1 MiB, 128 pages
	// Span lengths below this get an exact-size free list.
	inline constexpr Usize MaxPages = Usize{1} << (20 - PageShift);  // 128

	// Number of bits needed to hold a page id: 48 bit address space minus the page
	// shift.  tcmalloc uses the same value (internal/config.h: AddressBits).
	inline constexpr int AddressBits = 48;
	inline constexpr int PageIdBits = AddressBits - static_cast<int>(PageShift);

	// Thread cache tuning, straight out of tcmalloc's common.h.
	inline constexpr Usize MinThreadCacheSize = MaxSize * 2;
	inline constexpr Usize MaxThreadCacheSize = 4 << 20;
	inline constexpr Usize DefaultOverallThreadCacheSize = 8u * MaxThreadCacheSize;
	inline constexpr Usize StealAmount = Usize{1} << 16;

	// The number of times a deallocation may push a free list over its max length
	// before the max length is shrunk.
	inline constexpr int MaxOverages = 3;
	// Upper bound on a per-thread free list's dynamic max length.
	inline constexpr Usize MaxDynamicFreeListLength = 8192;

	static_assert(sizeof(void*) == 8, "PenMemory assumes a 64-bit address space");
	static_assert(PageSize % Alignment == 0, "page must be a multiple of alignment");
	static_assert(MinSystemAlloc % PageSize == 0, "chunk size must be page aligned");
	static_assert((MaxSize % PageSize) == 0, "MaxSize must be page aligned");
	static_assert(MaxObjectsToMove >= 2, "batches must move at least 2 objects");

	// ---------------------------------------------------------------------------
	// Assertion helpers (used by the tests and by the debug checks below).
	// ---------------------------------------------------------------------------
	inline constexpr bool IsAlignedTo(Usize value, Usize alignment)
	{
		return (value & (alignment - 1)) == 0;
	}
	inline constexpr bool IsAlignedTo(const void* p, Usize alignment)
	{
		return IsAlignedTo(reinterpret_cast<Usize>(p), alignment);
	}

	// ---------------------------------------------------------------------------
	// Length / PageId / Range.  Simplified versions of tcmalloc's pages.h.
	// ---------------------------------------------------------------------------
	class Length
	{
	public:
		constexpr Length() : m_pages(0) {}
		explicit constexpr Length(Usize n) : m_pages(n) {}

		constexpr Usize RawNum() const { return m_pages; }
		constexpr Usize InBytes() const { return m_pages * PageSize; }

		friend constexpr bool operator==(Length a, Length b) { return a.m_pages == b.m_pages; }
		friend constexpr bool operator!=(Length a, Length b) { return a.m_pages != b.m_pages; }
		friend constexpr bool operator<(Length a, Length b) { return a.m_pages < b.m_pages; }
		friend constexpr bool operator>(Length a, Length b) { return a.m_pages > b.m_pages; }
		friend constexpr bool operator<=(Length a, Length b) { return a.m_pages <= b.m_pages; }
		friend constexpr bool operator>=(Length a, Length b) { return a.m_pages >= b.m_pages; }
		friend constexpr Length operator+(Length a, Length b) { return Length(a.m_pages + b.m_pages); }
		friend constexpr Length operator-(Length a, Length b) { return Length(a.m_pages - b.m_pages); }
		friend constexpr Length operator*(Length a, Usize k) { return Length(a.m_pages * k); }

	private:
		Usize m_pages;
	};

	class PageId
	{
	public:
		constexpr PageId() : m_pageNumber(0) {}
		explicit constexpr PageId(Usize pn) : m_pageNumber(pn) {}

		constexpr Usize Index() const { return m_pageNumber; }
		constexpr Usize StartUIntPtr() const { return m_pageNumber << PageShift; }
		void* StartAddress() const { return reinterpret_cast<void*>(StartUIntPtr()); }

		friend constexpr bool operator==(PageId a, PageId b) { return a.m_pageNumber == b.m_pageNumber; }
		friend constexpr bool operator!=(PageId a, PageId b) { return a.m_pageNumber != b.m_pageNumber; }
		friend constexpr bool operator<(PageId a, PageId b) { return a.m_pageNumber < b.m_pageNumber; }
		friend constexpr bool operator<=(PageId a, PageId b) { return a.m_pageNumber <= b.m_pageNumber; }
		friend constexpr bool operator>=(PageId a, PageId b) { return a.m_pageNumber >= b.m_pageNumber; }
		friend constexpr PageId operator+(PageId p, Length n) { return PageId(p.m_pageNumber + n.RawNum()); }
		friend constexpr PageId operator-(PageId p, Length n) { return PageId(p.m_pageNumber - n.RawNum()); }
		friend constexpr Length operator-(PageId a, PageId b) { return Length(a.m_pageNumber - b.m_pageNumber); }

	private:
		Usize m_pageNumber;
	};

	struct Range
	{
		constexpr Range() = default;
		constexpr Range(PageId page, Length len) : Page(page), NumPages(len) {}
		PageId Page;
		Length NumPages;

		void* StartAddress() const { return Page.StartAddress(); }
		Usize InBytes() const { return NumPages.InBytes(); }
	};

	inline constexpr Length BytesToLengthCeil(Usize bytes)
	{
		return Length((bytes >> PageShift) + (IsAlignedTo(bytes, PageSize) ? 0 : 1));
	}
	inline constexpr Length BytesToLengthFloor(Usize bytes)
	{
		return Length(bytes >> PageShift);
	}
	inline PageId PageIdContaining(const void* p)
	{
		return PageId(reinterpret_cast<Usize>(p) >> PageShift);
	}

	// ---------------------------------------------------------------------------
	// SpinLock.  tcmalloc uses absl::base_internal::SpinLock; we need a very small
	// stand-in.  Never held across a call that can block for long.
	// ---------------------------------------------------------------------------
	inline void CpuPause()
	{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
		_mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
		__builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
		__asm__ __volatile__("yield");
#endif
	}

	class SpinLock
	{
	public:
		constexpr SpinLock() = default;
		SpinLock(const SpinLock&) = delete;
		SpinLock& operator=(const SpinLock&) = delete;

		void lock() noexcept
		{
			for (;;)
			{
				if (!m_locked.exchange(true, std::memory_order_acquire)) return;
				while (m_locked.load(std::memory_order_relaxed))
				{
					// Spin, but do not starve the lock holder.
					for (int i = 0; i < 64; ++i) CpuPause();
				}
			}
		}
		void unlock() noexcept { m_locked.store(false, std::memory_order_release); }

	private:
		std::atomic<bool> m_locked{false};
	};

	class SpinLockHolder
	{
	public:
		explicit SpinLockHolder(SpinLock* lock) : m_lock(lock) { m_lock->lock(); }
		~SpinLockHolder() { m_lock->unlock(); }
		SpinLockHolder(const SpinLockHolder&) = delete;
		SpinLockHolder& operator=(const SpinLockHolder&) = delete;

		void Unlock() { m_lock->unlock(); }
		void Lock() { m_lock->lock(); }

	private:
		SpinLock* m_lock;
	};

	// ---------------------------------------------------------------------------
	// Size classes.  Values copied from tcmalloc's size_classes.cc for
	// TCMALLOC_PAGE_SHIFT == 13 and __STDCPP_DEFAULT_NEW_ALIGNMENT__ > 8 (the
	// variant used on targets whose default new alignment is 16 bytes, which is
	// the case for x86-64 MSVC and for clang/gcc on x86-64 with aligned new).
	//
	//   Size      : maximum object size served by the class
	//   SpanBytes : bytes requested from the page heap per span
	//   Batch     : NumObjectsToMove (how many objects move between the
	//               thread cache and the central cache in one hop)
	// ---------------------------------------------------------------------------
	struct SizeClassInfo
	{
		U32 Size;
		U32 SpanBytes;
		U16 Batch;
	};

	inline constexpr SizeClassInfo SizeClassTable[NumClasses] =
	{
		//  bytes  SpanBytes Batch       objects/span
		{0, 0, 0},                 //  0  (not a size class)
		{8, 8192, 32},             //  1  1024
		{16, 8192, 32},            //  2   512
		{32, 8192, 32},            //  3   256
		{48, 8192, 32},            //  4   170
		{64, 8192, 32},            //  5   128
		{80, 8192, 32},            //  6   102
		{96, 8192, 32},            //  7    85
		{112, 8192, 32},           //  8    73
		{128, 8192, 32},           //  9    64
		{160, 8192, 32},           // 10    51
		{176, 8192, 32},           // 11    46
		{208, 8192, 32},           // 12    39
		{256, 8192, 32},           // 13    32
		{304, 8192, 32},           // 14    26
		{368, 8192, 32},           // 15    22
		{448, 8192, 32},           // 16    18
		{512, 8192, 32},           // 17    16
		{576, 8192, 32},           // 18    14
		{704, 8192, 32},           // 19    11
		{896, 8192, 32},           // 20     9
		{1024, 8192, 32},          // 21     8
		{1152, 16384, 32},         // 22    14
		{1408, 16384, 32},         // 23    11
		{1792, 16384, 32},         // 24     9
		{2048, 8192, 32},          // 25     4
		{2304, 16384, 28},         // 26     7
		{2688, 16384, 24},         // 27     6
		{3456, 24576, 18},         // 28     7
		{4096, 8192, 16},          // 29     2
		{4736, 24576, 13},         // 30     5
		{6144, 24576, 10},         // 31     4
		{8192, 8192, 8},           // 32     1
		{9472, 40960, 6},          // 33     4
		{12288, 24576, 5},         // 34     2
		{16384, 16384, 4},         // 35     1
		{20480, 40960, 3},         // 36     2
		{28672, 57344, 2},         // 37     2
		{32768, 32768, 2},         // 38     1
		{40960, 40960, 2},         // 39     1
		{49152, 49152, 2},         // 40     1
		{65536, 65536, 2},         // 41     1
		{73728, 73728, 2},         // 42     1
		{81920, 81920, 2},         // 43     1
		{98304, 98304, 2},         // 44     1
		{131072, 131072, 2},       // 45     1
		{155648, 155648, 2},       // 46     1
		{204800, 204800, 2},       // 47     1
		{262144, 262144, 2},       // 48     1
	};

	// Mapping between object sizes and size classes.  Same two-level flattening
	// trick as tcmalloc's SizeMap::ClassIndex: sizes <= 1024 are 8-byte aligned and
	// sizes > 1024 are 128-byte aligned, so both can share one flat array.
	class SizeMap
	{
	public:
		static constexpr Usize LargeSize = 1024;
		static constexpr Usize LargeSizeAlignment = 128;
		static constexpr Usize SmallSizeAlignment = 8;

		// Flattened array size: indices [0, 128] cover sizes (0, 1024] in steps of 8,
		// indices [129, 2168] cover sizes (1024, 262144] in steps of 128.
		static constexpr Usize ClassArraySize =
			LargeSize / SmallSizeAlignment +
			(MaxSize - LargeSize) / LargeSizeAlignment + 1;

		constexpr SizeMap() = default;

		void Init()
		{
			for (Usize c = 0; c < NumClasses; ++c)
			{
				const SizeClassInfo& info = SizeClassTable[c];
				PEN_MEMORY_CHECK(info.SpanBytes % PageSize == 0);
				PEN_MEMORY_CHECK(c == 0 || info.Size > SizeClassTable[c - 1].Size);
				PEN_MEMORY_CHECK(c == 0 || info.SpanBytes / info.Size >= 1);
				PEN_MEMORY_CHECK(info.Batch <= MaxObjectsToMove);
				PEN_MEMORY_CHECK(info.SpanBytes / PageSize <= MaxPages);
				m_classToSize[c] = info.Size;
				m_classToPages[c] = static_cast<U8>(info.SpanBytes / PageSize);
				m_numObjectsToMove[c] = static_cast<U8>(info.Batch);
			}
			PEN_MEMORY_CHECK(SizeClassTable[NumClasses - 1].Size == MaxSize);

			// Fill the canonical mapping array: every size maps to the smallest class
			// whose object size is >= size.
			for (Usize c = 1, s = 0; c < NumClasses && s <= MaxSize; ++c)
			{
				for (; s <= m_classToSize[c]; s += SmallSizeAlignment)
				{
					m_classArray[ClassIndex(s)] = static_cast<U8>(c);
				}
			}
			PEN_MEMORY_CHECK(m_classArray[0] == 1);
			PEN_MEMORY_CHECK(ClassIndex(MaxSize) == ClassArraySize - 1);
		}

		// Index into m_classArray for a size no greater than MaxSize.
		static constexpr Usize ClassIndex(Usize s)
		{
			if (s <= LargeSize)
			{
				// Ceil(s / 8).
				return (s + SmallSizeAlignment - 1) / SmallSizeAlignment;
			}
			// Ceil(s / 128) shifted past the small-size portion of the array.  The
			// constant folds (LargeSize/8 - LargeSize/128) * 128 == 120 * 128.
			return ((s + LargeSizeAlignment - 1 +
				(LargeSize / SmallSizeAlignment - LargeSize / LargeSizeAlignment) *
					LargeSizeAlignment) /
				LargeSizeAlignment);
		}

		// Smallest class able to hold `size`.  Returns 0 when size > MaxSize.
		PEN_MEMORY_ALWAYS_INLINE Usize SizeClass(Usize size) const
		{
			if (PEN_MEMORY_PREDICT_FALSE(size > MaxSize)) return 0;
			return m_classArray[ClassIndex(size)];
		}

		// Smallest class able to hold `size` whose object size is a multiple of
		// `align`.  Returns 0 when the request cannot be satisfied by a size class.
		//
		// tcmalloc's SizeMap::GetSizeClass does exactly this: because spans are
		// page-aligned and objects are laid out at multiples of the class size, a
		// class whose size is a multiple of `align` guarantees aligned objects.
		Usize SizeClassAligned(Usize size, Usize align) const
		{
			Usize c = SizeClass(size);
			if (PEN_MEMORY_PREDICT_FALSE(c == 0)) return 0;
			while (!IsAlignedTo(m_classToSize[c], align))
			{
				++c;
				if (PEN_MEMORY_PREDICT_FALSE(c >= NumClasses)) return 0;
			}
			return c;
		}

		PEN_MEMORY_ALWAYS_INLINE Usize ClassToSize(Usize c) const
		{
			PEN_MEMORY_ASSERT(c < NumClasses);
			return m_classToSize[c];
		}
		PEN_MEMORY_ALWAYS_INLINE Usize ClassToPages(Usize c) const
		{
			PEN_MEMORY_ASSERT(c < NumClasses);
			return m_classToPages[c];
		}
		PEN_MEMORY_ALWAYS_INLINE Usize NumObjectsToMove(Usize c) const
		{
			PEN_MEMORY_ASSERT(c < NumClasses);
			return m_numObjectsToMove[c];
		}
		// Number of objects a span of class `c` holds.
		Usize ObjectsPerSpan(Usize c) const
		{
			return ClassToPages(c) * PageSize / ClassToSize(c);
		}

	private:
		U8 m_classArray[ClassArraySize] = {};
		U32 m_classToSize[NumClasses] = {};
		U8 m_classToPages[NumClasses] = {};
		U8 m_numObjectsToMove[NumClasses] = {};
	};

}  // namespace PenMemory
