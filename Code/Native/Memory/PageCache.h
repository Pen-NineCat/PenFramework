// File /Native/Memory/PageCache.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: page cache (the innermost layer).
//
// Owns spans: contiguous runs of pages.  Everything the allocator hands out is
// ultimately carved out of this layer.
//
//   * New(n)      : return a span of n contiguous pages
//   * NewAligned() : return a span of n pages aligned to a page boundary
//   * Delete(s)   : give a span back; adjacent free spans are coalesced
//   * ReleaseAtLeastNPages() : hand physical pages back to the OS
//
// Modelled on tcmalloc's classic PageHeap (page_allocator.h before HPAA):
// per-length free lists plus a "large" list, neighbour coalescing through the
// page map, and growth in chunks of at least MinSystemAlloc.
//
// Deviations from tcmalloc (see DESIGN.md):
//   * released memory is decommitted and parked on a separate list instead of
//     being driven by a background release thread with a rate limit;
//   * no huge-page awareness, no per-partition page heaps.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "Common.hpp"
#include "OSMemory.h"
#include "Radix.h"

namespace PenMemory
{

// A span is either parked on a page-cache free list or handed out.
enum class SpanState : U8
{
	Free = 0,
	Allocated = 1,
};

// A contiguous run of pages.
//
// Small-object spans additionally carry the object free list for the central
// cache.  tcmalloc compresses that list into 2-byte object indexes plus an
// embedded array (see span.cc); we use the classic intrusive singly linked
// list built out of the free objects themselves, which is much easier to
// verify and is what tcmalloc used before the compressed representation.
class Span
{
	public:
	Span() = default;

	void Init(PageId first, Length numPages)
	{
		m_firstPage = first;
		m_numPages = static_cast<U32>(numPages.RawNum());
		m_sizeClass = 0;
		m_state = SpanState::Free;
		m_released = false;
		m_regionId = InvalidRegion;
		m_next = nullptr;
		m_prev = nullptr;
		m_freeList = nullptr;
		m_numObjects = 0;
		m_numAllocated = 0;
	}

	// Identifies the OS reservation (growth chunk) this span was carved from.
	// Spans of different regions are never merged, because releasing memory back
	// to the OS (MEM_DECOMMIT / madvise) has to stay inside one reservation.
	static constexpr U32 InvalidRegion = 0xffffffffu;
	U32 RegionId() const { return m_regionId; }
	void SetRegionId(U32 id) { m_regionId = id; }

	// -- geometry ------------------------------------------------------------
	PageId FirstPage() const { return m_firstPage; }
	PageId LastPage() const { return m_firstPage + Length(m_numPages - 1); }
	Length NumPages() const { return Length(m_numPages); }
	void SetNumPages(Length n) { m_numPages = static_cast<U32>(n.RawNum()); }
	void SetFirstPage(PageId p) { m_firstPage = p; }
	void* StartAddress() const { return m_firstPage.StartAddress(); }
	Usize BytesInSpan() const { return NumPages().InBytes(); }

	// -- ownership -----------------------------------------------------------
	// 0 for spans owned by the page cache or by a large (> MaxSize) allocation.
	Usize SizeClass() const { return m_sizeClass; }
	void SetSizeClass(Usize c) { m_sizeClass = static_cast<U32>(c); }
	SpanState State() const { return m_state; }
	void SetState(SpanState s) { m_state = s; }
	bool IsFree() const { return m_state == SpanState::Free; }
	bool IsAllocated() const { return m_state == SpanState::Allocated; }
	bool IsReleased() const { return m_released; }
	void SetReleased(bool v) { m_released = v; }

	// -- page-cache free list links -----------------------------------------
	Span* Next() const { return m_next; }
	void SetNext(Span* s) { m_next = s; }
	Span* Prev() const { return m_prev; }
	void SetPrev(Span* s) { m_prev = s; }

	// -- object free list (small-object spans, owned by the central cache) ---
	// Lays out `objectSize` objects over the whole span and links them all.
	void BuildFreelist(Usize objectSize)
	{
		PEN_MEMORY_CHECK(objectSize >= sizeof(void*));
		PEN_MEMORY_CHECK(BytesInSpan() / objectSize >= 1);
		m_freeList = nullptr;
		m_numAllocated = 0;
		m_numObjects = static_cast<U32>(BytesInSpan() / objectSize);
		char* base = static_cast<char*>(StartAddress());
		// Push in reverse so that the first pop returns the lowest address.
		for (Usize i = m_numObjects; i > 0; --i)
		{
			void* p = base + (i - 1) * objectSize;
			*reinterpret_cast<void**>(p) = m_freeList;
			m_freeList = p;
		}
	}

	// Removes up to N objects; returns how many were removed.
	int PopBatch(void** batch, int N, Usize objectSize)
	{
		(void)objectSize;
		int n = 0;
		while (n < N && m_freeList != nullptr)
		{
			void* p = m_freeList;
			m_freeList = *reinterpret_cast<void**>(p);
			batch[n++] = p;
		}
		m_numAllocated += static_cast<U32>(n);
		return n;
	}

	// Returns one object to the span.
	void Push(void* ptr, Usize objectSize)
	{
		PEN_MEMORY_CHECK(objectSize >= sizeof(void*));
		PEN_MEMORY_ASSERT(m_numAllocated > 0);
		PEN_MEMORY_ASSERT(ptr >= StartAddress() &&
										ptr < static_cast<char*>(StartAddress()) + BytesInSpan());
		*reinterpret_cast<void**>(ptr) = m_freeList;
		m_freeList = ptr;
		m_numAllocated--;
	}

	bool FreelistEmpty() const { return m_freeList == nullptr; }
	Usize NumAllocated() const { return m_numAllocated; }
	Usize NumObjects() const { return m_numObjects; }
	// Head of the intrusive free list (objects store their successor in their own
	// first word).  Only used for verification.
	void* FreeListHead() const { return m_freeList; }

	private:
	PageId m_firstPage;
	U32 m_numPages = 0;
	U32 m_sizeClass = 0;
	SpanState m_state = SpanState::Free;
	bool m_released = false;
	U32 m_regionId = InvalidRegion;
	Span* m_next = nullptr;
	Span* m_prev = nullptr;

	void* m_freeList = nullptr;
	U32 m_numObjects = 0;
	U32 m_numAllocated = 0;
};

class PageCache
{
	public:
	struct Stats
	{
		Usize SystemBytes = 0;    // obtained from the OS
		Usize FreeBytes = 0;      // parked on the free lists
		Usize ReleasedBytes = 0;  // decommitted, still tracked
		Usize AllocatedBytes = 0; // handed out (or held by the central cache)
		Usize SpanMetadataBytes = 0;  // Span objects + radix nodes
		Usize FreeSpans = 0;      // spans parked on the free/released lists
	};

	PageCache() = default;
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;

	void Init(PageMap* pageMap);

	// Allocate a run of `n` contiguous pages (n >= 1).  Returns nullptr when the
	// OS refuses to give us more memory.  The returned span is not registered
	// with any size class: it belongs to the caller until Delete().
	Span* New(Length n);

	// As New(), but the first page index is a multiple of `alignPages`.
	Span* NewAligned(Length n, Length alignPages);

	// Return a span obtained from New()/NewAligned().  Adjacent free spans are
	// coalesced and the result may be released back to the OS.
	void Delete(Span* span);

	// Decommit at least `n` pages of free memory and park them on the released
	// list.  Returns the number of pages actually released.
	Length ReleaseAtLeastNPages(Length n);

	// Release every free span (malloc_trim style).
	Length ReleaseAllFreePages() { return ReleaseAtLeastNPages(Length(Usize{1} << 40)); }

	// Automatic release kicks in once the free list exceeds this many pages.
	// 0 disables automatic release.
	void SetReleaseThreshold(Length pages)
	{
		SpinLockHolder h(&m_lock);
		m_releaseThresholdPages = pages.RawNum();
	}

	Stats GetStats();

	// Debug helper: verifies the free lists, the coalescing invariant, the page
	// map and the page accounting.  On failure *error points at a static string.
	bool Verify(const char** error);

	private:
	// Free lists are circular doubly linked lists with a sentinel Span.
	static bool ListEmpty(const Span* sentinel) { return sentinel->Next() == sentinel; }
	static void ListPush(Span* sentinel, Span* s)
	{
		s->SetNext(sentinel->Next());
		s->SetPrev(sentinel);
		sentinel->Next()->SetPrev(s);
		sentinel->SetNext(s);
	}
	static void ListUnlink(Span* s)
	{
		s->Prev()->SetNext(s->Next());
		s->Next()->SetPrev(s->Prev());
		s->SetNext(nullptr);
		s->SetPrev(nullptr);
	}
	static bool IsAlignedPage(PageId p, Length alignPages)
	{
		return alignPages.RawNum() <= 1 ||
						(p.Index() & (alignPages.RawNum() - 1)) == 0;
	}

	Span* SearchFreeLists(Length n, Length alignPages);
	// Same, but accepts any free span that *contains* an n-page window starting
	// on an aligned page, not only spans that are aligned at their first page.
	Span* SearchAlignedFreeSpan(Length n, Length alignPages);
	Span* Carve(Length n, Span* span);
	// Carves n pages starting at the first page of `span` that is a multiple of
	// alignPages, splitting off the unaligned head into its own free span.
	Span* CarveAligned(Length n, Length alignPages, Span* span);
	static bool ContainsAlignedWindow(const Span* span, Length n, Length alignPages);
	// Registers a new OS reservation and returns its id.
	U32 AddRegion(PageId first, Length pages);
	static bool SameRegion(const Span* a, const Span* b)
	{
		return a->RegionId() != Span::InvalidRegion &&
						a->RegionId() == b->RegionId();
	}
	void InsertFree(Span* span);
	void RemoveFree(Span* span);
	Span* MergeIntoFreeList(Span* span);
	Span* GrowHeap(Length n);
	bool ReviveReleased(Length n, Length alignPages);
	void MaybeRelease();
	Length ReleaseAtLeastNPagesLocked(Length n);
	void MapRange(Span* span);
	Span* AllocateSpanObject();
	void FreeSpanObject(Span* span);
	Span* GetDescriptor(PageId p) const { return m_pageMap->Get(p); }

	SpinLock m_lock;
	PageMap* m_pageMap = nullptr;

	// Description of every OS reservation handed to us by os::Allocate().
	struct RegionInfo
	{
		PageId first;
		Length pages;
	};
	RegionInfo* m_regions = nullptr;
	Usize m_regionCount = 0;
	Usize m_regionCapacity = 0;

	// Spans shorter than MaxPages live in m_freeLists[length - 1]; longer ones
	// live on m_largeList.  Both are sentinels.
	Span m_freeLists[MaxPages];
	Span m_largeList;
	// Decommitted spans.  They are kept out of the free lists so that coalescing
	// never has to reason about partially committed neighbours.
	Span m_releasedList;

	// Recycled Span objects.
	Span* m_spanObjectFreeList = nullptr;

	Usize m_systemPages = 0;
	Usize m_freePages = 0;
	Usize m_releasedPages = 0;
	Usize m_spanObjects = 0;
	Usize m_releaseThresholdPages = (Usize{8} << 20) / PageSize;  // 8 MiB
};

}  // namespace PenMemory
