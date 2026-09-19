// File /Native/Memory/Interface.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: the exported C ABI.
//
// The whole allocator is reachable through four functions:
//
//   void* Allocate(size_t size);
//   void  Deallocate(void* ptr);
//   void* AlignedAllocate(size_t alignment, size_t size);
//   void  AlignedDeallocate(void* ptr, size_t alignment);
//
// Deallocation does not need a size (or an alignment): the pointer is mapped to
// its span through the page map, and the span knows its size class.  This is the
// same contract as tcmalloc's free()/operator delete.
//
// This header is deliberately C compatible: it must stay includable from C and
// usable as a P/Invoke surface, so it only uses fixed width types and never
// exposes a C++ class (the internal C++ state lives in Globals.hpp).

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#ifdef PEN_MEMORY_BUILD
#define PEN_MEMORY_API __declspec(dllexport)
#elifdef PEN_MEMORY_IMPORT
#define PEN_MEMORY_API __declspec(dllimport)
#else
#define PEN_MEMORY_API
#endif
#else
#define PEN_MEMORY_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define PEN_MEMORY_NOEXCEPT noexcept
#else
#define PEN_MEMORY_NOEXCEPT
#endif

// Alignment every allocation is guaranteed to satisfy without asking for an
// aligned allocation.  A type whose alignof() is larger has to go through
// AlignedAllocate() (the C++ wrapper in Engine/Memory/Memory.hpp does that
// automatically).
#define PEN_MEMORY_BASIC_ALIGNMENT 8

#ifdef __cplusplus
extern "C"
{
#endif

	// -----------------------------------------------------------------------
	// Allocation
	// -----------------------------------------------------------------------

	// Allocates `size` bytes.  Returns NULL when out of memory (never throws).
	// A zero-sized request returns a unique, freeable pointer.
	PEN_MEMORY_API void* Allocate(size_t size) PEN_MEMORY_NOEXCEPT;

	// Frees a pointer returned by Allocate/AlignedAllocate.  Deallocate(NULL) is
	// a no-op.  Freeing anything else aborts with a diagnostic.
	PEN_MEMORY_API void Deallocate(void* ptr) PEN_MEMORY_NOEXCEPT;

	// Allocates `size` bytes aligned to `alignment` (a power of two).
	//   * alignment <= PEN_MEMORY_BASIC_ALIGNMENT : same as Allocate
	//   * alignment <= the allocator page size    : served by a size class whose
	//                                               object size is a multiple of
	//                                               `alignment`
	//   * larger                                  : served by a page-aligned span
	PEN_MEMORY_API void* AlignedAllocate(size_t alignment, size_t size) PEN_MEMORY_NOEXCEPT;

	// Frees a pointer returned by AlignedAllocate.  `alignment` is only used to
	// validate the pointer.
	PEN_MEMORY_API void AlignedDeallocate(void* ptr, size_t alignment) PEN_MEMORY_NOEXCEPT;

	// -----------------------------------------------------------------------
	// Introspection
	// -----------------------------------------------------------------------

	// Usable size of an allocation: the size class' object size for small
	// objects, the span size for page-level allocations.  0 for pointers we do
	// not own.
	PEN_MEMORY_API size_t GetAllocatedSize(const void* ptr);

	// Size a request of `size` bytes would actually occupy.
	PEN_MEMORY_API size_t GetEstimatedAllocatedSize(size_t size);

	// Heap statistics snapshot.  Every field is 64 bit so the layout is
	// identical for 32 and 64 bit callers.
	typedef struct PenMemoryStats
	{
		uint64_t ThreadCacheBytes;     // parked in per-thread free lists
		uint64_t ThreadCacheCount;     // live thread caches
		uint64_t CentralCacheBytes;    // parked in central free lists
		uint64_t CentralCacheSpans;    // spans owned by the central cache
		uint64_t SystemBytes;          // obtained from the OS
		uint64_t PageFreeBytes;        // on the page cache free lists
		uint64_t PageReleasedBytes;    // decommitted, still tracked
		uint64_t PageAllocatedBytes;   // handed out by the page cache
		uint64_t MetadataBytes;        // Span objects + page map nodes
	} PenMemoryStats;

	// Fills `stats` with a snapshot of the heap.  `stats` may be NULL.
	PEN_MEMORY_API void GetStats(PenMemoryStats* stats);

	// Hands free page-cache memory back to the OS.  Returns the number of pages
	// actually released.  Called automatically once the free lists grow past the
	// release threshold (see PageCache::SetReleaseThreshold).
	PEN_MEMORY_API size_t ReleaseFreeMemory(void);

	// Allocator page size (8 KiB).
	PEN_MEMORY_API size_t GetPageSize(void);

	// Alignment guaranteed by Allocate (PEN_MEMORY_BASIC_ALIGNMENT).
	PEN_MEMORY_API size_t GetAlignment(void);

	// Checks the internal invariants of every layer.  Returns non-zero when
	// everything is consistent; on failure *error points at a static
	// description.  Tests use it after every workload.
	PEN_MEMORY_API uint8_t VerifyHeap(const char** error);

#ifdef __cplusplus
}  // extern "C"
#endif
