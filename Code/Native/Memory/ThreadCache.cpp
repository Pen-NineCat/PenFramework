// File /Native/Memory/ThreadCache.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: per-thread cache (implementation).

#include "ThreadCache.h"

#include "Globals.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace PenMemory
{

	// ---------------------------------------------------------------------------
	// Static state
	// ---------------------------------------------------------------------------
	SpinLock ThreadCache::m_threadCacheLock;
	ThreadCache* ThreadCache::m_threadHeaps = nullptr;
	Usize ThreadCache::m_threadHeapCount = 0;
	ThreadCache* ThreadCache::m_nextMemorySteal = nullptr;
	std::atomic<Usize> ThreadCache::m_overallThreadCacheSize =
		DefaultOverallThreadCacheSize;
	Usize ThreadCache::m_perThreadCacheSize = MaxThreadCacheSize;
	I64 ThreadCache::m_unclaimedCacheSpace =
		static_cast<I64>(DefaultOverallThreadCacheSize);
	ThreadCache* ThreadCache::m_cacheFreeList = nullptr;
	bool ThreadCache::m_tsdInited = false;

	namespace
	{

		// ---------------------------------------------------------------------------
		// Platform thread-exit hook.  tcmalloc uses pthread_setspecific() for the same
		// purpose; FlsAlloc() is the Windows equivalent.
		// ---------------------------------------------------------------------------
#if defined(_WIN32)
		DWORD g_tlsIndex = TLS_OUT_OF_INDEXES;

		void NTAPI ThreadCacheTlsCallback(PVOID handle)
		{
			ThreadCache::DestroyThreadCache(static_cast<ThreadCache*>(handle));
		}

		void PlatformTlsInit() { g_tlsIndex = FlsAlloc(&ThreadCacheTlsCallback); }
		void PlatformTlsSet(ThreadCache* cache)
		{
			if (g_tlsIndex != TLS_OUT_OF_INDEXES)
			{
				FlsSetValue(g_tlsIndex, cache);
			}
		}
		U64 CurrentThreadId() { return static_cast<U64>(GetCurrentThreadId()); }
#else
		pthread_key_t g_tlsKey;
		bool g_tlsKeyValid = false;

		void ThreadCacheTlsCallback(void* handle)
		{
			ThreadCache::DestroyThreadCache(static_cast<ThreadCache*>(handle));
		}

		void PlatformTlsInit()
		{
			g_tlsKeyValid = pthread_key_create(&g_tlsKey, &ThreadCacheTlsCallback) == 0;
		}
		void PlatformTlsSet(ThreadCache* cache)
		{
			if (g_tlsKeyValid) pthread_setspecific(g_tlsKey, cache);
		}
		U64 CurrentThreadId() { return static_cast<U64>(pthread_self()); }
#endif

	}  // namespace

	void ThreadCache::InitTSD()
	{
		PEN_MEMORY_CHECK(!m_tsdInited);
		PlatformTlsInit();
		m_tsdInited = true;
	}

	ThreadCache* ThreadCache::NewCache(Globals* globals)
	{
		ThreadCache* cache = nullptr;
		if (m_cacheFreeList != nullptr)
		{
			cache = m_cacheFreeList;
			m_cacheFreeList = cache->m_next;
		}
		else
		{
			void* mem = os::MetaDataAlloc(sizeof(ThreadCache));
			if (mem == nullptr) return nullptr;
			cache = new (mem) ThreadCache();
		}

		// Re-initialize everything (a recycled cache carries stale state).
		for (Usize c = 0; c < NumClasses; ++c) cache->m_lists[c].Init();
		cache->m_size = 0;
		cache->m_maxSize = 0;
		cache->m_next = nullptr;
		cache->m_prev = nullptr;
		cache->m_sizeMap = &globals->SizeClasses;
		cache->m_centralCache = &globals->Central;

		// Claim a slice of the global thread cache budget.  m_threadCacheLock is held
		// by our caller.
		cache->IncreaseCacheLimitLocked();
		if (cache->m_maxSize == 0)
		{
			// There is not enough budget to go around; take the minimum anyway and let
			// the bookkeeping go negative, exactly like tcmalloc.
			cache->m_maxSize = MinThreadCacheSize;
			m_unclaimedCacheSpace -= static_cast<I64>(MinThreadCacheSize);
		}

		// Publish it.
		cache->m_next = m_threadHeaps;
		cache->m_prev = nullptr;
		if (m_threadHeaps != nullptr) m_threadHeaps->m_prev = cache;
		m_threadHeaps = cache;
		m_threadHeapCount++;
		if (m_nextMemorySteal == nullptr) m_nextMemorySteal = cache;
		return cache;
	}

	ThreadCache* ThreadCache::CreateCacheIfNecessary()
	{
		Globals& globals = Globals::Get();
		ThreadCache* cache = nullptr;

		{
			SpinLockHolder h(&m_threadCacheLock);
			if (g_tlsThreadCache != nullptr) return g_tlsThreadCache;
			if (!m_tsdInited) InitTSD();

			const U64 tid = CurrentThreadId();
			// A recursive allocation (metadata allocation inside NewCache) may already
			// have created this thread's cache, so look for it first.
			for (ThreadCache* c = m_threadHeaps; c != nullptr; c = c->m_next)
			{
				if (c->m_tid == tid)
				{
					cache = c;
					break;
				}
			}
			if (cache == nullptr)
			{
				cache = NewCache(&globals);
				if (cache != nullptr) cache->m_tid = tid;
			}
		}

		if (cache == nullptr) return nullptr;
		g_tlsThreadCache = cache;
		PlatformTlsSet(cache);
		return cache;
	}

	void ThreadCache::DestroyThreadCache(ThreadCache* cache)
	{
		if (cache == nullptr) return;
		g_tlsThreadCache = nullptr;
		PlatformTlsSet(nullptr);
		DeleteCache(cache);
	}

	void ThreadCache::BecomeIdle()
	{
		ThreadCache* cache = GetCacheIfPresent();
		if (cache == nullptr) return;
		DestroyThreadCache(cache);
	}

	void ThreadCache::DeleteCache(ThreadCache* heap)
	{
		// Return everything to the central cache first: this needs no thread cache
		// lock, so it cannot deadlock with a thread that is allocating.
		heap->Cleanup();

		SpinLockHolder h(&m_threadCacheLock);
		if (heap->m_next != nullptr) heap->m_next->m_prev = heap->m_prev;
		if (heap->m_prev != nullptr) heap->m_prev->m_next = heap->m_next;
		if (m_threadHeaps == heap) m_threadHeaps = heap->m_next;
		PEN_MEMORY_CHECK(m_threadHeapCount > 0);
		m_threadHeapCount--;

		if (m_nextMemorySteal == heap) m_nextMemorySteal = heap->m_next;
		if (m_nextMemorySteal == nullptr) m_nextMemorySteal = m_threadHeaps;
		m_unclaimedCacheSpace += static_cast<I64>(heap->m_maxSize);

		// Recycle the object.
		heap->m_next = m_cacheFreeList;
		heap->m_prev = nullptr;
		m_cacheFreeList = heap;
	}

	// ---------------------------------------------------------------------------
	// Batch movement between thread cache and central cache
	// ---------------------------------------------------------------------------
	void ThreadCache::Cleanup()
	{
		for (Usize sizeClass = 1; sizeClass < NumClasses; ++sizeClass)
		{
			if (!m_lists[sizeClass].IsEmpty())
			{
				ReleaseToCentralCache(&m_lists[sizeClass], sizeClass,
					static_cast<int>(m_lists[sizeClass].Count()));
			}
		}
	}

	void* ThreadCache::FetchFromCentralCache(Usize sizeClass, Usize byteSize)
	{
		FreeList* list = &m_lists[sizeClass];
		PEN_MEMORY_ASSERT(list->IsEmpty());
		const Usize batchSize = m_sizeMap->NumObjectsToMove(sizeClass);
		const int numToMove =
			static_cast<int>(std::min<Usize>(list->MaxLength(), batchSize));

		void* batch[MaxObjectsToMove];
		const int fetchCount = m_centralCache->RemoveRange(sizeClass, batch, numToMove);
		if (PEN_MEMORY_PREDICT_FALSE(fetchCount == 0)) return nullptr;

		if (fetchCount > 1)
		{
			m_size += byteSize * static_cast<Usize>(fetchCount - 1);
			list->PushBatch(fetchCount - 1, batch + 1);
		}

		// Slow start the max length up to batchSize, then grow in batchSize steps so
		// that the length stays a multiple of the batch size.
		if (list->MaxLength() < batchSize)
		{
			list->SetMaxLength(list->MaxLength() + 1);
		}
		else
		{
			Usize newLength =
				std::min(list->MaxLength() + batchSize, MaxDynamicFreeListLength);
			newLength -= newLength % batchSize;
			PEN_MEMORY_ASSERT(newLength % batchSize == 0);
			list->SetMaxLength(newLength);
		}
		return batch[0];
	}

	void ThreadCache::ListTooLong(FreeList* list, Usize sizeClass)
	{
		const int batchSize =
			static_cast<int>(m_sizeMap->NumObjectsToMove(sizeClass));
		ReleaseToCentralCache(list, sizeClass, batchSize);

		// Make MaxLength converge on batchSize.
		if (list->MaxLength() < static_cast<Usize>(batchSize))
		{
			// Slow start, so we do not over-reserve.
			list->SetMaxLength(list->MaxLength() + 1);
		}
		else if (list->MaxLength() > static_cast<Usize>(batchSize))
		{
			// If we consistently go over MaxLength, shrink it.
			list->SetLengthOverages(list->LengthOverages() + 1);
			if (list->LengthOverages() > MaxOverages)
			{
				PEN_MEMORY_ASSERT(list->MaxLength() > static_cast<Usize>(batchSize));
				list->SetMaxLength(list->MaxLength() - static_cast<Usize>(batchSize));
				list->SetLengthOverages(0);
			}
		}
	}

	void ThreadCache::DeallocateSlow(void* ptr, FreeList* list, Usize sizeClass)
	{
		(void)ptr;
		if (PEN_MEMORY_PREDICT_FALSE(list->Count() > list->MaxLength()))
		{
			ListTooLong(list, sizeClass);
		}
		if (m_size >= m_maxSize) Scavenge();
	}

	void ThreadCache::ReleaseToCentralCache(FreeList* src, Usize sizeClass, int N)
	{
		PEN_MEMORY_ASSERT(src == &m_lists[sizeClass]);
		if (N <= 0) return;
		if (static_cast<Usize>(N) > src->Count()) N = static_cast<int>(src->Count());
		if (N <= 0) return;
		const Usize deltaBytes = static_cast<Usize>(N) * m_sizeMap->ClassToSize(sizeClass);

		// Return prepackaged chains of the right size.
		void* batch[MaxObjectsToMove];
		const int batchSize =
			static_cast<int>(m_sizeMap->NumObjectsToMove(sizeClass));
		while (N > batchSize)
		{
			src->PopBatch(batchSize, batch);
			m_centralCache->InsertRange(sizeClass, batch, batchSize);
			N -= batchSize;
		}
		src->PopBatch(N, batch);
		m_centralCache->InsertRange(sizeClass, batch, N);
		m_size -= deltaBytes;
	}

	// ---------------------------------------------------------------------------
	// Budget management
	// ---------------------------------------------------------------------------
	void ThreadCache::IncreaseCacheLimit()
	{
		SpinLockHolder h(&m_threadCacheLock);
		IncreaseCacheLimitLocked();
	}

	void ThreadCache::IncreaseCacheLimitLocked()
	{
		if (m_unclaimedCacheSpace > 0)
		{
			// May push m_unclaimedCacheSpace negative; that is intentional.
			m_unclaimedCacheSpace -= static_cast<I64>(StealAmount);
			m_maxSize += StealAmount;
			return;
		}
		// Steal from at most 10 other thread caches, then give up.
		for (int i = 0; i < 10; ++i, m_nextMemorySteal = m_nextMemorySteal->m_next)
		{
			if (m_nextMemorySteal == nullptr)
			{
				PEN_MEMORY_ASSERT(m_threadHeaps != nullptr);
				m_nextMemorySteal = m_threadHeaps;
			}
			if (m_nextMemorySteal == this ||
				m_nextMemorySteal->m_maxSize <= MinThreadCacheSize)
			{
				continue;
			}
			m_nextMemorySteal->m_maxSize -= StealAmount;
			m_maxSize += StealAmount;
			m_nextMemorySteal = m_nextMemorySteal->m_next;
			return;
		}
	}

	void ThreadCache::Scavenge()
	{
		// If the low-water mark is L, we would not have needed to touch the central
		// cache had the list been L shorter.  Drop L/2 nodes to converge on that.
		for (Usize sizeClass = 1; sizeClass < NumClasses; ++sizeClass)
		{
			FreeList* list = &m_lists[sizeClass];
			const Usize lowMark = list->LowWatermark();
			if (lowMark > 0)
			{
				const int drop = lowMark > 1 ? static_cast<int>(lowMark / 2) : 1;
				ReleaseToCentralCache(list, sizeClass, drop);

				const int batchSize =
					static_cast<int>(m_sizeMap->NumObjectsToMove(sizeClass));
				if (list->MaxLength() > static_cast<Usize>(batchSize))
				{
					list->SetMaxLength(std::max<Usize>(
						list->MaxLength() - static_cast<Usize>(batchSize),
						static_cast<Usize>(batchSize)));
				}
			}
			list->ClearLowWatermark();
		}
		IncreaseCacheLimit();
	}

	void ThreadCache::RecomputePerThreadCacheSize()
	{
		const Usize n = m_threadHeapCount > 0 ? m_threadHeapCount : 1;
		Usize space = m_overallThreadCacheSize.load(std::memory_order_relaxed) / n;

		if (space < MinThreadCacheSize) space = MinThreadCacheSize;
		if (space > MaxThreadCacheSize) space = MaxThreadCacheSize;

		const double ratio =
			static_cast<double>(space) / static_cast<double>(std::max<Usize>(1, m_perThreadCacheSize));
		Usize claimed = 0;
		for (ThreadCache* h = m_threadHeaps; h != nullptr; h = h->m_next)
		{
			// Increasing the total must not circumvent the slow start of m_maxSize.
			if (ratio < 1.0)
			{
				h->m_maxSize = static_cast<Usize>(static_cast<double>(h->m_maxSize) * ratio);
			}
			claimed += h->m_maxSize;
		}
		m_unclaimedCacheSpace = static_cast<I64>(
			m_overallThreadCacheSize.load(std::memory_order_relaxed) - claimed);
		m_perThreadCacheSize = space;
	}

	void ThreadCache::SetOverallThreadCacheSize(Usize newSize)
	{
		if (newSize < MinThreadCacheSize) newSize = MinThreadCacheSize;
		SpinLockHolder h(&m_threadCacheLock);
		m_overallThreadCacheSize.store(newSize, std::memory_order_relaxed);
		RecomputePerThreadCacheSize();
	}

	ThreadCache::Stats ThreadCache::GetStats()
	{
		SpinLockHolder h(&m_threadCacheLock);
		Stats stats;
		for (ThreadCache* c = m_threadHeaps; c != nullptr; c = c->m_next)
		{
			stats.Bytes += c->m_size;
			stats.CacheCount++;
			for (Usize s = 1; s < NumClasses; ++s)
			{
				stats.Objects[s] += c->m_lists[s].Count();
			}
		}
		return stats;
	}

}  // namespace PenMemory
