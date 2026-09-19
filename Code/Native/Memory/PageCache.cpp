// File /Native/Memory/PageCache.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: page cache (implementation).
//
// Invariants maintained by this file:
//
//   * Every page of every span obtained from the OS is registered in the page
//     map and points at the Span currently owning it.  Because of that, an
//     arbitrary interior pointer can be attributed to its span.
//   * A span is either on exactly one free list (state == Free), on the
//     released list (state == Free && IsReleased()), or handed out
//     (state == Allocated).
//   * Free spans never sit adjacent to each other: Delete() and GrowHeap()
//     coalesce.
//   * m_freePages counts the pages on the free lists; m_releasedPages counts
//     decommitted pages; both are disjoint from each other and from memory
//     handed out.

#include "PageCache.h"

#include <cstring>

namespace PenMemory
{

void PageCache::Init(PageMap* pageMap)
{
	PEN_MEMORY_CHECK(m_pageMap == nullptr);
	m_pageMap = pageMap;
	for (Usize i = 0; i < MaxPages; ++i)
	{
		m_freeLists[i].SetNext(&m_freeLists[i]);
		m_freeLists[i].SetPrev(&m_freeLists[i]);
	}
	m_largeList.SetNext(&m_largeList);
	m_largeList.SetPrev(&m_largeList);
	m_releasedList.SetNext(&m_releasedList);
	m_releasedList.SetPrev(&m_releasedList);
}

// ---------------------------------------------------------------------------
// Free list plumbing (lock held)
// ---------------------------------------------------------------------------
void PageCache::InsertFree(Span* span)
{
	if (span->NumPages().RawNum() < MaxPages)
	{
		ListPush(&m_freeLists[span->NumPages().RawNum() - 1], span);
	}
	else
	{
		ListPush(&m_largeList, span);
	}
	m_freePages += span->NumPages().RawNum();
}

void PageCache::RemoveFree(Span* span)
{
	PEN_MEMORY_ASSERT(!ListEmpty(span));
	ListUnlink(span);
	m_freePages -= span->NumPages().RawNum();
}

// Writes the page map for every page of `span`.  Registering all pages (rather
// than just the first, as tcmalloc does) is what lets us recognise interior
// pointers and double frees.
void PageCache::MapRange(Span* span)
{
	const PageId last = span->LastPage();
	for (PageId p = span->FirstPage(); p <= last; p = p + Length(1))
	{
		m_pageMap->Set(p, span);
	}
}

Span* PageCache::AllocateSpanObject()
{
	if (m_spanObjectFreeList != nullptr)
	{
		Span* s = m_spanObjectFreeList;
		m_spanObjectFreeList = s->Next();
		*s = Span();
		return s;
	}
	void* mem = os::MetaDataAlloc(sizeof(Span));
	if (mem == nullptr) return nullptr;
	m_spanObjects++;
	return new (mem) Span();
}

void PageCache::FreeSpanObject(Span* span)
{
	span->SetNext(m_spanObjectFreeList);
	span->SetPrev(nullptr);
	m_spanObjectFreeList = span;
}

// ---------------------------------------------------------------------------
// Free list search and carving (lock held)
// ---------------------------------------------------------------------------
Span* PageCache::SearchFreeLists(Length n, Length alignPages)
{
	// 1) Exact match, the common case (a 1-page span for a small size class).
	if (n.RawNum() <= MaxPages && alignPages.RawNum() <= 1)
	{
		Span* sentinel = &m_freeLists[n.RawNum() - 1];
		if (!ListEmpty(sentinel)) return sentinel->Next();
	}
	// 2) Best fit over the exact-size lists.
	for (Usize len = n.RawNum(); len <= MaxPages; ++len)
	{
		Span* sentinel = &m_freeLists[len - 1];
		for (Span* s = sentinel->Next(); s != sentinel; s = s->Next())
		{
			if (IsAlignedPage(s->FirstPage(), alignPages)) return s;
		}
	}
	// 3) Best effort over the large list.  Long spans are kept in insertion
	//    order, so the length has to be checked here.
	for (Span* s = m_largeList.Next(); s != &m_largeList; s = s->Next())
	{
		if (s->NumPages() >= n && IsAlignedPage(s->FirstPage(), alignPages))
		{
			return s;
		}
	}
	return nullptr;
}

Usize AlignedStartPage(Usize page, Usize align)
{
	return (page + align - 1) & ~(align - 1);
}

bool PageCache::ContainsAlignedWindow(const Span* span, Length n,
																			Length alignPages)
{
	const Usize align = alignPages.RawNum();
	const Usize first = span->FirstPage().Index();
	const Usize skip = AlignedStartPage(first, align) - first;
	return span->NumPages().RawNum() >= skip + n.RawNum();
}

Span* PageCache::SearchAlignedFreeSpan(Length n, Length alignPages)
{
	// tcmalloc only accepts spans that are aligned at their first page and retries
	// after growing; we additionally accept any free span containing an aligned
	// window, which makes large alignments work without growing the heap.
	for (Usize len = n.RawNum(); len <= MaxPages; ++len)
	{
		Span* sentinel = &m_freeLists[len - 1];
		for (Span* s = sentinel->Next(); s != sentinel; s = s->Next())
		{
			if (ContainsAlignedWindow(s, n, alignPages)) return s;
		}
	}
	for (Span* s = m_largeList.Next(); s != &m_largeList; s = s->Next())
	{
		if (ContainsAlignedWindow(s, n, alignPages)) return s;
	}
	return nullptr;
}

Span* PageCache::Carve(Length n, Span* span)
{
	PEN_MEMORY_ASSERT(span->IsFree());
	PEN_MEMORY_ASSERT(span->NumPages() >= n);
	const U32 region = span->RegionId();
	RemoveFree(span);

	if (span->NumPages() > n)
	{
		Span* rest = AllocateSpanObject();
		if (rest == nullptr)
		{
			// Metadata exhausted: put the span back and fail the allocation.
			InsertFree(span);
			return nullptr;
		}
		rest->Init(span->FirstPage() + n, span->NumPages() - n);
		rest->SetRegionId(region);
		span->SetNumPages(n);
		MapRange(rest);
		InsertFree(rest);
	}

	span->SetState(SpanState::Allocated);
	PEN_MEMORY_ASSERT(!span->IsReleased());
	// The carved span now owns exactly its own pages.
	MapRange(span);
	return span;
}

Span* PageCache::CarveAligned(Length n, Length alignPages, Span* span)
{
	const Usize align = alignPages.RawNum();
	const Usize first = span->FirstPage().Index();
	const Usize aligned = AlignedStartPage(first, align);
	if (aligned != first)
	{
		const Usize skip = aligned - first;
		PEN_MEMORY_ASSERT(ContainsAlignedWindow(span, n, alignPages));
		Span* head = AllocateSpanObject();
		if (head == nullptr) return nullptr;
		const U32 region = span->RegionId();
		RemoveFree(span);
		head->Init(span->FirstPage(), Length(skip));
		head->SetRegionId(region);
		span->SetFirstPage(PageId(aligned));
		span->SetNumPages(span->NumPages() - Length(skip));
		MapRange(head);
		InsertFree(head);
		// `span` is free and adjacent to its own head for the next few
		// instructions.  It leaves the free lists again in Carve() below without
		// dropping the lock, so no other thread can observe that state.
		InsertFree(span);
	}
	return Carve(n, span);
}

// ---------------------------------------------------------------------------
// Growth and coalescing (lock held)
// ---------------------------------------------------------------------------
U32 PageCache::AddRegion(PageId first, Length pages)
{
	if (m_regionCount == m_regionCapacity)
	{
		const Usize newCapacity = m_regionCapacity == 0 ? 64 : m_regionCapacity * 2;
		RegionInfo* grown = static_cast<RegionInfo*>(
				os::MetaDataAlloc(sizeof(RegionInfo) * newCapacity));
		if (grown == nullptr) return Span::InvalidRegion;
		if (m_regions != nullptr)
		{
			std::memcpy(grown, m_regions, sizeof(RegionInfo) * m_regionCount);
		}
		m_regions = grown;
		m_regionCapacity = newCapacity;
	}
	m_regions[m_regionCount] = RegionInfo{first, pages};
	return static_cast<U32>(m_regionCount++);
}

Span* PageCache::GrowHeap(Length n)
{
	PEN_MEMORY_CHECK(n.RawNum() > 0);
	const Length minimumChunk(MinSystemAlloc / PageSize);

	Length chunk = std::max(n, minimumChunk);
	void* mem = os::Allocate(chunk.InBytes(), PageSize);
	if (mem == nullptr && chunk > n)
	{
		// The OS refused the full chunk; try to satisfy just this request.
		chunk = n;
		mem = os::Allocate(chunk.InBytes(), PageSize);
	}
	if (mem == nullptr) return nullptr;

	Span* span = AllocateSpanObject();
	if (span == nullptr)
	{
		os::Deallocate(mem, chunk.InBytes());
		return nullptr;
	}

	const PageId first = PageIdContaining(mem);
	span->Init(first, chunk);
	if (!m_pageMap->Ensure(Range(first, chunk)))
	{
		// Address does not fit in the page map (should not happen below 2^48).
		os::Deallocate(mem, chunk.InBytes());
		FreeSpanObject(span);
		return nullptr;
	}
	const U32 region = AddRegion(first, chunk);
	if (region == Span::InvalidRegion)
	{
		os::Deallocate(mem, chunk.InBytes());
		FreeSpanObject(span);
		return nullptr;
	}
	span->SetRegionId(region);

	m_systemPages += chunk.RawNum();
	MapRange(span);
	// The OS frequently hands back memory adjacent to an existing region, so go
	// through the coalescing path rather than a plain insert.  (Regions are never
	// merged with each other: see Span::RegionId.)
	return MergeIntoFreeList(span);
}

Span* PageCache::MergeIntoFreeList(Span* span)
{
	span->SetState(SpanState::Free);

	// Coalesce with the span immediately before, when it is free, adjacent, and
	// belongs to the same OS reservation.
	if (span->FirstPage().Index() > 0)
	{
		Span* prev = GetDescriptor(span->FirstPage() - Length(1));
		if (prev != nullptr && prev->IsFree() && !prev->IsReleased() &&
				prev->LastPage() + Length(1) == span->FirstPage() &&
				SameRegion(prev, span))
		{
			RemoveFree(prev);
			span->SetFirstPage(prev->FirstPage());
			span->SetNumPages(span->NumPages() + prev->NumPages());
			FreeSpanObject(prev);
		}
	}

	// ... and with the span immediately after.
	Span* next = GetDescriptor(span->LastPage() + Length(1));
	if (next != nullptr && next->IsFree() && !next->IsReleased() &&
			next->FirstPage() == span->LastPage() + Length(1) &&
			SameRegion(next, span))
	{
		RemoveFree(next);
		span->SetNumPages(span->NumPages() + next->NumPages());
		FreeSpanObject(next);
	}

	MapRange(span);
	InsertFree(span);
	return span;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------
Span* PageCache::New(Length n)
{
	PEN_MEMORY_CHECK(n.RawNum() > 0);
	SpinLockHolder h(&m_lock);

	Span* span = SearchFreeLists(n, Length(1));
	if (span == nullptr && ReviveReleased(n, Length(1)))
	{
		span = SearchFreeLists(n, Length(1));
	}
	if (span == nullptr)
	{
		if (GrowHeap(std::max(n, Length(MinSystemAlloc / PageSize))) == nullptr)
		{
			return nullptr;
		}
		span = SearchFreeLists(n, Length(1));
	}
	if (span == nullptr) return nullptr;
	return Carve(n, span);
}

Span* PageCache::NewAligned(Length n, Length alignPages)
{
	PEN_MEMORY_CHECK(n.RawNum() > 0);
	if (alignPages.RawNum() <= 1) return New(n);
	PEN_MEMORY_CHECK((alignPages.RawNum() & (alignPages.RawNum() - 1)) == 0);

	SpinLockHolder h(&m_lock);
	// A free region of n + alignPages - 1 contiguous pages always contains an
	// aligned window of n pages, so a bounded number of attempts must succeed.
	for (int attempt = 0; attempt < 4; ++attempt)
	{
		Span* span = SearchAlignedFreeSpan(n, alignPages);
		if (span != nullptr) return CarveAligned(n, alignPages, span);
		if (ReviveReleased(n, alignPages)) continue;
		const Length grow =
				std::max(n + alignPages, Length(MinSystemAlloc / PageSize));
		if (GrowHeap(grow) == nullptr) return nullptr;
	}
	return nullptr;
}

void PageCache::Delete(Span* span)
{
	PEN_MEMORY_CHECK(span != nullptr);
	PEN_MEMORY_CHECK_MSG(span->SizeClass() == 0,
										"span must be unregistered before it goes back to the page "
										"cache (size_class=%zu)",
										span->SizeClass());
	SpinLockHolder h(&m_lock);
	if (PEN_MEMORY_PREDICT_FALSE(span->IsFree() || span->IsReleased()))
	{
		ReportCorruptedFree(span->StartAddress(), "double free of a page span");
	}
	MergeIntoFreeList(span);
	MaybeRelease();
}

bool PageCache::ReviveReleased(Length n, Length alignPages)
{
	for (Span* s = m_releasedList.Next(); s != &m_releasedList; s = s->Next())
	{
		if (s->NumPages() < n) continue;
		if (!ContainsAlignedWindow(s, n, alignPages)) continue;

		ListUnlink(s);
		m_releasedPages -= s->NumPages().RawNum();
		if (!os::Commit(s->StartAddress(), s->BytesInSpan()))
		{
			// Could not re-commit: put it back and report failure.
			ListPush(&m_releasedList, s);
			m_releasedPages += s->NumPages().RawNum();
			return false;
		}
		s->SetReleased(false);
		MergeIntoFreeList(s);
		return true;
	}
	return false;
}

Length PageCache::ReleaseAtLeastNPagesLocked(Length n)
{
	Length released;
	// Releasing a span can fail (for example when it would cross two OS
	// reservations); such a span simply stays in the free lists.
	auto tryRelease = [&](Span* s)
	{
		RemoveFree(s);
		if (!os::Decommit(s->StartAddress(), s->BytesInSpan()))
		{
			InsertFree(s);
			return;
		}
		s->SetReleased(true);
		ListPush(&m_releasedList, s);
		m_releasedPages += s->NumPages().RawNum();
		released = released + s->NumPages();
	};

	// Release the longest spans first: they are the least likely to be needed
	// again soon and give back the most memory per syscall.
	for (Span* s = m_largeList.Next(); s != &m_largeList && released < n;)
	{
		Span* next = s->Next();
		tryRelease(s);
		s = next;
	}
	for (Usize len = MaxPages; len >= 1 && released < n; --len)
	{
		Span* sentinel = &m_freeLists[len - 1];
		// Advance through the list once.  A failed release pushes the span back to
		// the front of this very list, so the successor is captured up front.
		for (Span* s = sentinel->Next(); s != sentinel && released < n;)
		{
			Span* next = s->Next();
			tryRelease(s);
			s = next;
		}
	}
	return released;
}

Length PageCache::ReleaseAtLeastNPages(Length n)
{
	SpinLockHolder h(&m_lock);
	return ReleaseAtLeastNPagesLocked(n);
}

void PageCache::MaybeRelease()
{
	if (m_releaseThresholdPages == 0) return;
	if (m_freePages <= m_releaseThresholdPages) return;
	// Hand back half of what we are sitting on: enough to matter, while keeping
	// a healthy amount of memory for reuse.
	ReleaseAtLeastNPagesLocked(Length(m_freePages / 2));
}

PageCache::Stats PageCache::GetStats()
{
	SpinLockHolder h(&m_lock);
	Stats stats;
	stats.SystemBytes = m_systemPages * PageSize;
	stats.FreeBytes = m_freePages * PageSize;
	stats.ReleasedBytes = m_releasedPages * PageSize;
	PEN_MEMORY_CHECK(m_systemPages >= m_freePages + m_releasedPages);
	stats.AllocatedBytes = (m_systemPages - m_freePages - m_releasedPages) * PageSize;
	stats.SpanMetadataBytes =
			m_spanObjects * sizeof(Span) + m_pageMap->BytesUsed();
	Usize spans = 0;
	for (Usize len = 1; len <= MaxPages; ++len)
	{
		for (Span* s = m_freeLists[len - 1].Next(); s != &m_freeLists[len - 1];
					s = s->Next())
		{
			spans++;
		}
	}
	for (Span* s = m_largeList.Next(); s != &m_largeList; s = s->Next()) spans++;
	for (Span* s = m_releasedList.Next(); s != &m_releasedList; s = s->Next()) spans++;
	stats.FreeSpans = spans;
	return stats;
}

bool PageCache::Verify(const char** error)
{
	const char* sink = nullptr;
	if (error == nullptr) error = &sink;
	SpinLockHolder h(&m_lock);
	*error = "";

	Usize countedFree = 0;
	Usize countedReleased = 0;

	// A free span must be mapped to itself on every page, must not carry a size
	// class, and must not have a free neighbour (those get coalesced).
	auto checkFreeSpan = [&](Span* s, bool released) -> bool
	{
		if (!s->IsFree() || s->IsReleased() != released)
		{
			*error = "span on a free list has an inconsistent state";
			return false;
		}
		if (s->SizeClass() != 0)
		{
			*error = "span with a size class on a page cache free list";
			return false;
		}
		for (PageId p = s->FirstPage(); p <= s->LastPage(); p = p + Length(1))
		{
			if (m_pageMap->Get(p) != s)
			{
				*error = "free span is not mapped to itself in the page map";
				return false;
			}
		}
		if (!released)
		{
			if (s->FirstPage().Index() > 0)
			{
				Span* prev = m_pageMap->Get(s->FirstPage() - Length(1));
				if (prev != nullptr && prev->IsFree() && !prev->IsReleased() &&
						prev->LastPage() + Length(1) == s->FirstPage() &&
						SameRegion(prev, s))
				{
					*error = "adjacent free spans were not coalesced";
					return false;
				}
			}
			Span* next = m_pageMap->Get(s->LastPage() + Length(1));
			if (next != nullptr && next->IsFree() && !next->IsReleased() &&
					next->FirstPage() == s->LastPage() + Length(1) &&
					SameRegion(next, s))
			{
				*error = "adjacent free spans were not coalesced";
				return false;
			}
		}
		return true;
	};

	for (Usize len = 1; len <= MaxPages; ++len)
	{
		Span* sentinel = &m_freeLists[len - 1];
		for (Span* s = sentinel->Next(); s != sentinel; s = s->Next())
		{
			if (s->NumPages().RawNum() != len)
			{
				*error = "span sits in the wrong length bucket";
				return false;
			}
			if (!checkFreeSpan(s, /*released=*/false)) return false;
			countedFree += len;
		}
	}
	for (Span* s = m_largeList.Next(); s != &m_largeList; s = s->Next())
	{
		if (s->NumPages().RawNum() < MaxPages)
		{
			*error = "short span on the large list";
			return false;
		}
		if (!checkFreeSpan(s, /*released=*/false)) return false;
		countedFree += s->NumPages().RawNum();
	}
	for (Span* s = m_releasedList.Next(); s != &m_releasedList; s = s->Next())
	{
		if (!checkFreeSpan(s, /*released=*/true)) return false;
		countedReleased += s->NumPages().RawNum();
	}

	if (countedFree != m_freePages)
	{
		*error = "free page accounting is wrong";
		return false;
	}
	if (countedReleased != m_releasedPages)
	{
		*error = "released page accounting is wrong";
		return false;
	}
	if (m_freePages + m_releasedPages > m_systemPages)
	{
		*error = "more free pages than pages obtained from the OS";
		return false;
	}
	return true;
}

}  // namespace PenMemory
