// File /Native/Memory/CentralCache.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: central cache (implementation).

#include "CentralCache.h"

namespace PenMemory
{
	Span* CentralFreeList::AllocateSpan()
	{
		Span* span = m_pageCache->New(Length(m_pagesPerSpan));
		if (span == nullptr) return nullptr;

		span->SetSizeClass(m_sizeClass);
		span->BuildFreelist(m_objectSize);

		SpinLockHolder h(&m_lock);
		m_numSpans++;
		m_numFreeObjects += m_objectsPerSpan;
		return span;
	}

	void CentralFreeList::ReleaseSpan(Span* span)
	{
		PEN_MEMORY_ASSERT(span->NumAllocated() == 0);
		span->SetSizeClass(0);
		m_pageCache->Delete(span);
	}

	int CentralFreeList::Populate(void** batch, int N)
	{
		// Drop the central lock while talking to the page cache: this is what keeps
		// the lock order simple (central lock is never held while taking the page
		// heap lock) and is exactly what tcmalloc does.
		m_lock.unlock();
		Span* span = AllocateSpan();
		if (span == nullptr)
		{
			m_lock.lock();
			return 0;
		}
		const int got = span->PopBatch(batch, N, m_objectSize);
		PEN_MEMORY_CHECK(got > 0);
		m_lock.lock();

		m_numFreeObjects -= static_cast<Usize>(got);
		if (!span->FreelistEmpty())
		{
			// The span still has something to give: keep it around.
			ListPush(&m_nonEmpty, span);
		}
		return got;
	}

	int CentralFreeList::RemoveRange(void** batch, int N)
	{
		PEN_MEMORY_CHECK(N > 0 && static_cast<Usize>(N) <= MaxObjectsToMove);

		// Size classes with a single object per span bypass the freelist entirely
		// (tcmalloc does the same): the span *is* the object.
		if (PEN_MEMORY_PREDICT_FALSE(m_objectsPerSpan == 1))
		{
			Span* span = AllocateSpan();
			if (span == nullptr) return 0;
			const int got = span->PopBatch(batch, 1, m_objectSize);
			PEN_MEMORY_CHECK(got == 1);
			SpinLockHolder h(&m_lock);
			m_numFreeObjects -= 1;
			return 1;
		}

		int result = 0;
		SpinLockHolder h(&m_lock);
		while (result < N)
		{
			if (!ListEmpty(&m_nonEmpty))
			{
				Span* span = m_nonEmpty.Next();
				const int got = span->PopBatch(batch + result, N - result, m_objectSize);
				PEN_MEMORY_CHECK(got > 0);
				result += got;
				m_numFreeObjects -= static_cast<Usize>(got);
				if (span->FreelistEmpty()) ListUnlink(span);
				continue;
			}
			// Nothing cached: grab a fresh span.  Populate() drops the lock.
			const int got = Populate(batch + result, N - result);
			result += got;
			if (got == 0) break;  // out of memory
		}
		return result;
	}

	void CentralFreeList::InsertRange(void** batch, int N)
	{
		PEN_MEMORY_CHECK(N > 0 && static_cast<Usize>(N) <= MaxObjectsToMove);

		// Map the objects to their spans outside the lock, like tcmalloc does: the
		// page map lookup is read-only and can be the expensive part (cache misses).
		Span* spans[MaxObjectsToMove];
		for (int i = 0; i < N; ++i)
		{
			Span* span = m_pageMap->Get(PageIdContaining(batch[i]));
			if (PEN_MEMORY_PREDICT_FALSE(span == nullptr ||
					 span->SizeClass() != m_sizeClass))
			{
				ReportCorruptedFree(batch[i],
									"object does not belong to this size class");
			}
			spans[i] = span;
		}

		Span* freeSpans[MaxObjectsToMove];
		int freeCount = 0;
		{
			SpinLockHolder h(&m_lock);
			for (int i = 0; i < N; ++i)
			{
				Span* span = spans[i];
				const bool wasEmpty = span->FreelistEmpty();
				span->Push(batch[i], m_objectSize);
				m_numFreeObjects++;
				if (span->NumAllocated() == 0)
				{
					// The span is completely free now: remove it from the nonempty list and
					// schedule it for the page cache.
					if (!wasEmpty) ListUnlink(span);
					m_numFreeObjects -= span->NumObjects();
					m_numSpans--;
					freeSpans[freeCount++] = span;
				}
				else if (wasEmpty)
				{
					// The span became non-full again.
					ListPush(&m_nonEmpty, span);
				}
			}
		}

		// Handing spans back to the page cache takes the page heap lock; do it after
		// releasing the central lock.
		for (int i = 0; i < freeCount; ++i) ReleaseSpan(freeSpans[i]);
	}

	bool CentralFreeList::Verify(const char** error)
	{
		const char* sink = nullptr;
		if (error == nullptr) error = &sink;
		SpinLockHolder h(&m_lock);
		Usize listedSpans = 0;
		Usize freeObjects = 0;

		for (Span* s = m_nonEmpty.Next(); s != &m_nonEmpty; s = s->Next())
		{
			listedSpans++;
			if (s->SizeClass() != m_sizeClass)
			{
				*error = "span on the nonempty list belongs to another size class";
				return false;
			}
			if (!s->IsAllocated())
			{
				*error = "span on the nonempty list is not owned by the central cache";
				return false;
			}
			if (s->NumObjects() != m_objectsPerSpan)
			{
				*error = "span has the wrong object count for its size class";
				return false;
			}
			for (PageId p = s->FirstPage(); p <= s->LastPage(); p = p + Length(1))
			{
				if (m_pageMap->Get(p) != s)
				{
					*error = "central cache span is not mapped to itself in the page map";
					return false;
				}
			}

			// Walk the intrusive object free list.  A cycle or a length mismatch means
			// somebody handed the same object out twice (or freed one twice).
			Usize freeInSpan = 0;
			for (void* p = s->FreeListHead(); p != nullptr;
		 p = *reinterpret_cast<void**>(p))
			{
				freeInSpan++;
				if (freeInSpan > s->NumObjects())
				{
					*error = "cycle in a span object free list";
					return false;
				}
			}
			if (freeInSpan == 0)
			{
				*error = "span without free objects is still on the nonempty list";
				return false;
			}
			if (freeInSpan != s->NumObjects() - s->NumAllocated())
			{
				*error = "span object accounting is wrong";
				return false;
			}
			freeObjects += freeInSpan;
		}

		if (freeObjects != m_numFreeObjects)
		{
			*error = "central free list object accounting is wrong";
			return false;
		}
		if (listedSpans > m_numSpans)
		{
			*error = "more spans on the nonempty list than the size class owns";
			return false;
		}
		return true;
	}

	bool CentralCache::Verify(const char** error)
	{
		if (error != nullptr) *error = "";
		for (Usize c = 1; c < NumClasses; ++c)
		{
			if (!m_freeLists[c].Verify(error)) return false;
		}
		return true;
	}

}  // namespace PenMemory
