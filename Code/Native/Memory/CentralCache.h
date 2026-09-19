// File /Native/Memory/CentralCache.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: central cache.
//
// One freelist per size class, shared by all threads.  Objects move between a
// thread cache and the central cache in batches whose size comes from the size
// class table (NumObjectsToMove), which amortises the lock over many
// allocations.
//
// Structurally this is tcmalloc's CentralFreeList with the TransferCache folded
// in: tcmalloc puts a per-CPU/L3 batch cache (transfer_cache.h) in front of the
// central freelist, and a three-layer design has no room for it.  The
// observable behaviour is the same, only the batch is always taken straight
// from the spans.
//
// Flow:
//   RemoveRange : pop objects from spans on `m_nonEmpty`; when it runs out,
//                 Populate() asks the page cache for a fresh span, lays out the
//                 objects and puts the span on `m_nonEmpty`.
//   InsertRange : push each object back into the span that owns it (found via
//                 the page map); a span that becomes completely free goes back
//                 to the page cache.

#pragma once

#include <algorithm>
#include <cstddef>

#include "Common.hpp"
#include "PageCache.h"
#include "Radix.h"

namespace PenMemory
{
	class CentralFreeList
	{
	public:
		CentralFreeList() = default;
		CentralFreeList(const CentralFreeList&) = delete;
		CentralFreeList& operator=(const CentralFreeList&) = delete;

		void Init(Usize sizeClass, const SizeMap* sizeMap, PageCache* pageCache,
		  PageMap* pageMap)
		{
			m_sizeClass = sizeClass;
			m_objectSize = sizeMap->ClassToSize(sizeClass);
			m_pagesPerSpan = sizeMap->ClassToPages(sizeClass);
			m_objectsPerSpan = m_pagesPerSpan * PageSize / m_objectSize;
			m_sizeMap = sizeMap;
			m_pageCache = pageCache;
			m_pageMap = pageMap;
			m_nonEmpty.SetNext(&m_nonEmpty);
			m_nonEmpty.SetPrev(&m_nonEmpty);
			PEN_MEMORY_CHECK(m_objectsPerSpan >= 1);
		}

		// Removes up to N objects into `batch`; returns how many were removed.
		// Returns 0 only when the page cache is out of memory.
		[[nodiscard]] int RemoveRange(void** batch, int N);

		// Returns N objects (0 < N <= MaxObjectsToMove) that this size class handed
		// out earlier.  Cross-thread frees are fine: the objects are attributed to
		// their span through the page map.
		void InsertRange(void** batch, int N);

		// Free objects currently held in this size class' spans.
		Usize CachedObjectCount() const
		{
			SpinLockHolder h(&const_cast<SpinLock&>(m_lock));
			return m_numFreeObjects;
		}
		// Spans currently owned by this size class.
		Usize NumSpans() const
		{
			SpinLockHolder h(&const_cast<SpinLock&>(m_lock));
			return m_numSpans;
		}
		Usize SizeClass() const { return m_sizeClass; }
		Usize ObjectSize() const { return m_objectSize; }
		Usize ObjectsPerSpan() const { return m_objectsPerSpan; }

		// Debug helper: checks the span list, the object free lists and the object
		// accounting of this size class.
		bool Verify(const char** error);

	private:
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

		// Fetches a fresh span from the page cache and builds its object freelist.
		// Must be called with m_lock released; takes it internally for bookkeeping.
		Span* AllocateSpan();

		// Hands a fully free span back to the page cache.  Called with m_lock
		// released.
		void ReleaseSpan(Span* span);

		// Grabs a new span to refill `batch`.  Called with m_lock held: the lock is
		// dropped around the page cache call, exactly like tcmalloc's Populate().
		int Populate(void** batch, int N);

		SpinLock m_lock;
		Usize m_sizeClass = 0;
		Usize m_objectSize = 0;
		Usize m_pagesPerSpan = 0;
		Usize m_objectsPerSpan = 0;
		Usize m_numSpans = 0;
		Usize m_numFreeObjects = 0;
		const SizeMap* m_sizeMap = nullptr;
		PageCache* m_pageCache = nullptr;
		PageMap* m_pageMap = nullptr;
		// Spans that still have free objects.  tcmalloc keeps NumLists buckets
		// ordered by how full a span is, so that nearly-empty spans are drained
		// first; a single list is enough here.
		Span m_nonEmpty;
	};

	class CentralCache
	{
	public:
		CentralCache() = default;
		CentralCache(const CentralCache&) = delete;
		CentralCache& operator=(const CentralCache&) = delete;

		void Init(const SizeMap* sizeMap, PageCache* pageCache, PageMap* pageMap)
		{
			for (Usize c = 1; c < NumClasses; ++c)
			{
				m_freeLists[c].Init(c, sizeMap, pageCache, pageMap);
			}
		}

		PEN_MEMORY_ALWAYS_INLINE int RemoveRange(Usize sizeClass, void** batch, int N)
		{
			PEN_MEMORY_ASSERT(sizeClass > 0 && sizeClass < NumClasses);
			return m_freeLists[sizeClass].RemoveRange(batch, N);
		}
		PEN_MEMORY_ALWAYS_INLINE void InsertRange(Usize sizeClass, void** batch, int N)
		{
			PEN_MEMORY_ASSERT(sizeClass > 0 && sizeClass < NumClasses);
			m_freeLists[sizeClass].InsertRange(batch, N);
		}

		const CentralFreeList& FreeListOf(Usize sizeClass) const
		{
			return m_freeLists[sizeClass];
		}

		// Total bytes cached in central free lists.
		Usize CachedBytes()
		{
			Usize bytes = 0;
			for (Usize c = 1; c < NumClasses; ++c)
			{
				bytes += m_freeLists[c].CachedObjectCount() * m_freeLists[c].ObjectSize();
			}
			return bytes;
		}
		Usize LiveSpans()
		{
			Usize spans = 0;
			for (Usize c = 1; c < NumClasses; ++c) spans += m_freeLists[c].NumSpans();
			return spans;
		}

		// Debug helper: verifies every size class.
		bool Verify(const char** error);

	private:
		CentralFreeList m_freeLists[NumClasses];
	};

}  // namespace PenMemory
