// File /Native/Memory/ThreadCache.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: per-thread cache (the outermost layer).
//
// Each thread owns one ThreadCache: an array of free lists indexed by size
// class, plus the bookkeeping that keeps those lists from growing without
// bound.  The fast path is a plain pop/push on a thread-local list: no atomic
// RMW, no lock, no system call.
//
// Structure and policy are taken from tcmalloc's thread_cache.h/.cc:
//   * `MaxLength` grows slowly (slow start) and shrinks when a list keeps
//     overflowing, which is what makes the batch size adapt to each thread's
//     access pattern;
//   * `Scavenge()` gives back half of each list's low-water mark;
//   * the total amount of memory parked in thread caches is bounded by
//     m_overallThreadCacheSize, and a thread that needs more steals it from
//     another thread cache.
//
// Thread exit cleanup uses a platform TLS slot (FlsAlloc / pthread_key_create)
// whose destructor callback hands every cached object back to the central
// cache.  The hot path reads a separate constant-initialized `thread_local`
// pointer, so no dynamic TLS initializer ever runs inside the allocator.

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "CentralCache.h"
#include "Common.hpp"
#include "OSMemory.h"

namespace PenMemory
{

	class Globals;
	class ThreadCache;

	// Thread-local pointer to this thread's cache.  Constant-initialized: reading it
	// costs one TLS load and never runs a dynamic initializer.
	inline thread_local ThreadCache* g_tlsThreadCache = nullptr;

	// Objects store their successor in their own first word, so a free list needs
	// no extra storage.
	PEN_MEMORY_ALWAYS_INLINE void* SllNext(void* p)
	{
		return *reinterpret_cast<void**>(p);
	}
	PEN_MEMORY_ALWAYS_INLINE void SllSetNext(void* p, void* next)
	{
		*reinterpret_cast<void**>(p) = next;
	}

	class ThreadCache
	{
	public:
		// One free list per size class.
		class FreeList
		{
		public:
			void Init()
			{
				m_head = nullptr;
				m_length = 0;
				m_lowWater = 0;
				m_maxLength = 1;
				m_lengthOverages = 0;
			}

			Usize Count() const { return m_length; }
			bool IsEmpty() const { return m_head == nullptr; }

			// Dynamic cap on the list length; converges on NumObjectsToMove().
			Usize MaxLength() const { return m_maxLength; }
			void SetMaxLength(Usize n) { m_maxLength = static_cast<U32>(n); }
			Usize LengthOverages() const { return m_lengthOverages; }
			void SetLengthOverages(Usize n) { m_lengthOverages = static_cast<U32>(n); }
			Usize LowWatermark() const { return m_lowWater; }
			void ClearLowWatermark() { m_lowWater = m_length; }

			PEN_MEMORY_ALWAYS_INLINE bool TryPop(void** ret)
			{
				void* obj = m_head;
				if (PEN_MEMORY_PREDICT_FALSE(obj == nullptr)) return false;
				m_head = SllNext(obj);
				m_length--;
				if (PEN_MEMORY_PREDICT_FALSE(m_length < m_lowWater)) m_lowWater = m_length;
				*ret = obj;
				return true;
			}

			PEN_MEMORY_ALWAYS_INLINE void Push(void* ptr)
			{
				SllSetNext(ptr, m_head);
				m_head = ptr;
				m_length++;
			}

			// PushBatch/PopBatch do not preserve any particular order.
			void PushBatch(int N, void** batch)
			{
				PEN_MEMORY_ASSERT(N > 0);
				for (int i = 0; i + 1 < N; ++i) SllSetNext(batch[i], batch[i + 1]);
				SllSetNext(batch[N - 1], m_head);
				m_head = batch[0];
				m_length += static_cast<U32>(N);
			}

			void PopBatch(int N, void** batch)
			{
				void* p = m_head;
				for (int i = 0; i < N; ++i)
				{
					batch[i] = p;
					p = SllNext(p);
				}
				m_head = p;
				PEN_MEMORY_ASSERT(m_length >= static_cast<U32>(N));
				m_length -= static_cast<U32>(N);
				if (m_length < m_lowWater) m_lowWater = m_length;
			}

		private:
			void* m_head = nullptr;
			U32 m_length = 0;
			U32 m_lowWater = 0;
			U32 m_maxLength = 1;
			U32 m_lengthOverages = 0;
		};

		// Returns this thread's cache, creating it on first use.  Returns nullptr
		// only if the allocator cannot obtain metadata memory.
		static PEN_MEMORY_ALWAYS_INLINE ThreadCache* GetCache()
		{
			ThreadCache* cache = g_tlsThreadCache;
			return PEN_MEMORY_PREDICT_TRUE(cache != nullptr) ? cache
															: CreateCacheIfNecessary();
		}
		static PEN_MEMORY_ALWAYS_INLINE ThreadCache* GetCacheIfPresent()
		{
			return g_tlsThreadCache;
		}

		PEN_MEMORY_ALWAYS_INLINE void* Allocate(Usize size_class);
		PEN_MEMORY_ALWAYS_INLINE void Deallocate(void* ptr, Usize size_class);

		// Hands `cache` back to the allocator; called by the platform TLS destructor
		// when a thread exits, and by BecomeIdle().
		static void DestroyThreadCache(ThreadCache* cache);
		// Drops this thread's cache without exiting the thread.
		static void BecomeIdle();

		struct Stats
		{
			Usize Bytes = 0;		// bytes parked in all thread caches
			Usize CacheCount = 0;	// live thread caches
			Usize Objects[NumClasses] = {};	// free objects per size class
		};
		static Stats GetStats();

		static Usize OverallThreadCacheSize()
		{
			return m_overallThreadCacheSize.load(std::memory_order_relaxed);
		}
		static void SetOverallThreadCacheSize(Usize newSize);

		Usize Size() const { return m_size; }
		Usize MaxSize() const { return m_maxSize; }

	private:
		friend class ThreadCacheTestPeer;

		static ThreadCache* CreateCacheIfNecessary();
		static void InitTSD();

		// Returns everything this cache holds to the central cache.
		void Cleanup();

		void* FetchFromCentralCache(Usize size_class, Usize byteSize);
		void ListTooLong(FreeList* list, Usize size_class);
		void DeallocateSlow(void* ptr, FreeList* list, Usize size_class);
		void ReleaseToCentralCache(FreeList* src, Usize size_class, int N);
		void Scavenge();
		void IncreaseCacheLimit();
		// As above, with m_threadCacheLock already held.
		void IncreaseCacheLimitLocked();

		static ThreadCache* NewCache(Globals* globals);
		static void DeleteCache(ThreadCache* heap);
		static void RecomputePerThreadCacheSize();

		// The most frequently used fields come first.
		FreeList m_lists[NumClasses];

		Usize m_size = 0;		  // bytes parked in m_lists
		Usize m_maxSize = 0;	  // m_size > m_maxSize triggers Scavenge()
		U64 m_tid = 0;
		const SizeMap* m_sizeMap = nullptr;
		CentralCache* m_centralCache = nullptr;

		// Linked list of all thread caches (stats, memory stealing).
		ThreadCache* m_next = nullptr;
		ThreadCache* m_prev = nullptr;

		static SpinLock m_threadCacheLock;
		static ThreadCache* m_threadHeaps;
		static Usize m_threadHeapCount;
		static ThreadCache* m_nextMemorySteal;
		static std::atomic<Usize> m_overallThreadCacheSize;
		static Usize m_perThreadCacheSize;
		// m_overallThreadCacheSize minus the sum of m_maxSize over all caches.
		static I64 m_unclaimedCacheSpace;
		// Recycled ThreadCache objects.
		static ThreadCache* m_cacheFreeList;
		static bool m_tsdInited;
	};

	// Fast path.
	PEN_MEMORY_ALWAYS_INLINE void* ThreadCache::Allocate(Usize size_class)
	{
		PEN_MEMORY_ASSERT(size_class > 0 && size_class < NumClasses);
		const Usize allocatedSize = m_sizeMap->ClassToSize(size_class);
		FreeList* list = &m_lists[size_class];
		void* ret;
		if (PEN_MEMORY_PREDICT_TRUE(list->TryPop(&ret)))
		{
			m_size -= allocatedSize;
			return ret;
		}
		return FetchFromCentralCache(size_class, allocatedSize);
	}

	PEN_MEMORY_ALWAYS_INLINE void ThreadCache::Deallocate(void* ptr, Usize size_class)
	{
		PEN_MEMORY_ASSERT(size_class > 0 && size_class < NumClasses);
		FreeList* list = &m_lists[size_class];
		m_size += m_sizeMap->ClassToSize(size_class);

		// Both checks are folded into one branch, like tcmalloc does.
		const I64 sizeHeadroom =
			static_cast<I64>(m_maxSize) - static_cast<I64>(m_size) - 1;
		list->Push(ptr);
		const I64 listHeadroom = static_cast<I64>(list->MaxLength()) -
				 static_cast<I64>(list->Count());
		if (PEN_MEMORY_PREDICT_FALSE((listHeadroom | sizeHeadroom) < 0))
		{
			DeallocateSlow(ptr, list, size_class);
		}
	}

}  // namespace PenMemory
