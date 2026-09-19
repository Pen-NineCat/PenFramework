// File /Native/Memory/Interface.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: public interface (implementation).
//
// Allocation decides between the cached (size-class) path and the page-level
// path exactly like tcmalloc's fast_alloc()/slow_alloc_large():
//
//   size <= MaxSize : ThreadCache -> CentralCache -> PageCache
//   size >  MaxSize : PageCache (span start address *is* the allocation)
//
// Deallocation is size-less: page -> Span -> size class.  A span with size
// class 0 is either a page-level allocation or a span currently parked in the
// page cache (which is how double frees are detected).

#include "Interface.h"

#include <bit>

#include "Globals.hpp"

namespace PenMemory
{
	namespace
	{
		// Page-level allocation, used for requests above MaxSize and for alignments
		// above the page size.
		void* AllocatePages(Length pages, Length alignPages)
		{
			PEN_MEMORY_CHECK(pages.RawNum() > 0);
			Globals& globals = Globals::Get();
			Span* span = alignPages.RawNum() > 1
			   ? globals.Pages.NewAligned(pages, alignPages)
			   : globals.Pages.New(pages);
			if (span == nullptr) return nullptr;
			PEN_MEMORY_CHECK(span->SizeClass() == 0);
			PEN_MEMORY_CHECK(span->IsAllocated());
			return span->StartAddress();
		}
	}

	Globals::Globals()
	{
		SizeClasses.Init();
		Pages.Init(&PageMaps);
		Central.Init(&SizeClasses, &Pages, &PageMaps);
	}

	Globals& Globals::Get()
	{
		// Thread-safe one-time construction.  The constructor only wires up the
		// layers; it never allocates, so it cannot re-enter the allocator.
		static Globals instance;
		return instance;
	}
}

// The exported C ABI has to live at global scope; pulling the namespace in keeps
// the definitions below readable.
using namespace PenMemory;

extern "C"
{
	// -----------------------------------------------------------------------
	// Allocate / Deallocate
	// -----------------------------------------------------------------------
	void* Allocate(size_t size) noexcept
	{
		if (size == 0) size = 1;

		if (PEN_MEMORY_PREDICT_TRUE(size <= MaxSize))
		{
			Globals& globals = Globals::Get();
			const Usize sizeClass = globals.SizeClasses.SizeClass(size);
			ThreadCache* cache = ThreadCache::GetCache();
			if (PEN_MEMORY_PREDICT_FALSE(cache == nullptr)) return nullptr;
			return cache->Allocate(sizeClass);
		}

		return AllocatePages(BytesToLengthCeil(size), Length(1));
	}

	void Deallocate(void* ptr) noexcept
	{
		if (ptr == nullptr) return;

		Globals& globals = Globals::Get();
		Span* span = globals.PageMaps.Get(PageIdContaining(ptr));
		if (PEN_MEMORY_PREDICT_FALSE(span == nullptr))
		{
			ReportCorruptedFree(ptr, "pointer was never handed out");
		}

		const Usize sizeClass = span->SizeClass();
		if (PEN_MEMORY_PREDICT_TRUE(sizeClass != 0))
		{
			// Small object: back to this thread's cache for that size class.
#ifndef NDEBUG
			const Usize objectSize = globals.SizeClasses.ClassToSize(sizeClass);
			const Usize offset = reinterpret_cast<Usize>(ptr) -
				 reinterpret_cast<Usize>(span->StartAddress());
			PEN_MEMORY_ASSERT_MSG(offset < span->BytesInSpan() && offset % objectSize == 0,
				  "pointer does not start an object of size class %zu", sizeClass);
#endif
			ThreadCache* cache = ThreadCache::GetCache();
			if (PEN_MEMORY_PREDICT_TRUE(cache != nullptr))
			{
				cache->Deallocate(ptr, sizeClass);
			}
			else
			{
				// Could not create a thread cache; go straight to the central cache.
				globals.Central.InsertRange(sizeClass, &ptr, 1);
			}
			return;
		}

		// Page-level allocation.
		if (PEN_MEMORY_PREDICT_FALSE(span->IsFree()))
		{
			ReportCorruptedFree(ptr, "double free (span is already on a page cache free list)");
		}
		if (PEN_MEMORY_PREDICT_FALSE(ptr != span->StartAddress()))
		{
			ReportCorruptedFree(ptr, "not the start of a page-level allocation");
		}
		globals.Pages.Delete(span);
	}

	// -----------------------------------------------------------------------
	// AlignedAllocate / AlignedDeallocate
	// -----------------------------------------------------------------------
	void* AlignedAllocate(size_t alignment, size_t size) noexcept
	{
		if (size == 0) size = 1;
		if (alignment <= Alignment) return Allocate(size);
		if (PEN_MEMORY_PREDICT_FALSE(!std::has_single_bit(alignment))) return nullptr;

		Globals& globals = Globals::Get();

		// Requests that fit in a size class are served by picking a class whose
		// object size is a multiple of the alignment: spans are page aligned and
		// objects sit at multiples of the class size, so those objects are aligned.
		if (alignment <= PageSize && size <= MaxSize)
		{
			const Usize sizeClass = globals.SizeClasses.SizeClassAligned(size, alignment);
			if (PEN_MEMORY_PREDICT_FALSE(sizeClass == 0)) return nullptr;
			ThreadCache* cache = ThreadCache::GetCache();
			if (PEN_MEMORY_PREDICT_FALSE(cache == nullptr)) return nullptr;
			return cache->Allocate(sizeClass);
		}

		// Everything else comes from an aligned run of pages.
		const Length alignPages = BytesToLengthCeil(alignment);
		return AllocatePages(BytesToLengthCeil(size), alignPages.RawNum() > 0 ? alignPages
									   : Length(1));
	}

	void AlignedDeallocate(void* ptr, size_t alignment) noexcept
	{
		if (ptr == nullptr) return;
		if (alignment != 0 && std::has_single_bit(alignment) && !IsAlignedTo(ptr, alignment))
		{
			ReportCorruptedFree(ptr, "pointer does not respect the requested alignment");
		}
		Deallocate(ptr);
	}

	// -----------------------------------------------------------------------
	// Introspection
	// -----------------------------------------------------------------------
	size_t GetAllocatedSize(const void* ptr)
	{
		if (ptr == nullptr) return 0;
		Globals& globals = Globals::Get();
		Span* span = globals.PageMaps.Get(PageIdContaining(ptr));
		if (span == nullptr || span->IsFree()) return 0;
		const Usize sizeClass = span->SizeClass();
		if (sizeClass != 0) return globals.SizeClasses.ClassToSize(sizeClass);
		if (ptr != span->StartAddress()) return 0;
		return span->BytesInSpan();
	}

	size_t GetEstimatedAllocatedSize(size_t size)
	{
		if (size == 0) size = 1;
		Globals& globals = Globals::Get();
		if (size <= MaxSize)
		{
			return globals.SizeClasses.ClassToSize(globals.SizeClasses.SizeClass(size));
		}
		return BytesToLengthCeil(size).InBytes();
	}

	void GetStats(PenMemoryStats* stats)
	{
		if (stats == nullptr) return;

		Globals& globals = Globals::Get();
		PenMemoryStats result{};

		// Each layer is sampled under its own lock, so the numbers are a close but
		// not an atomic snapshot of the heap.
		ThreadCache::Stats threadCache = ThreadCache::GetStats();
		result.ThreadCacheBytes = threadCache.Bytes;
		result.ThreadCacheCount = threadCache.CacheCount;
		result.CentralCacheBytes = globals.Central.CachedBytes();
		result.CentralCacheSpans = globals.Central.LiveSpans();

		PageCache::Stats pageCache = globals.Pages.GetStats();
		result.SystemBytes = pageCache.SystemBytes;
		result.PageFreeBytes = pageCache.FreeBytes;
		result.PageReleasedBytes = pageCache.ReleasedBytes;
		result.PageAllocatedBytes = pageCache.AllocatedBytes;
		result.MetadataBytes = pageCache.SpanMetadataBytes + os::MetaDataBytes();

		*stats = result;
	}

	size_t ReleaseFreeMemory(void)
	{
		return Globals::Get().Pages.ReleaseAllFreePages().RawNum();
	}

	size_t GetPageSize(void) { return PageSize; }

	size_t GetAlignment(void) { return Alignment; }

	uint8_t VerifyHeap(const char** error)
	{
		static const char* const Ok = "";
		Globals& globals = Globals::Get();
		if (error != nullptr) *error = Ok;
		if (!globals.Pages.Verify(error)) return 0;
		if (!globals.Central.Verify(error)) return 0;
		return 1;
	}
}  // extern "C"
