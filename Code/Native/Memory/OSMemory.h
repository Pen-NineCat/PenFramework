// File /Native/Memory/OSMemory.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: OS memory primitives.
//
// Thin, allocator-agnostic wrappers around the platform's virtual memory
// interface.  This is the only file that knows about VirtualAlloc / mmap.
//
//   Windows : VirtualAlloc / VirtualFree / MEM_COMMIT / MEM_DECOMMIT
//   POSIX   : mmap / munmap / madvise
//
// It also hosts the metadata arena: everything the allocator needs for its own
// bookkeeping (radix tree nodes, Span objects, ThreadCache objects) is carved
// out of it, so metadata allocation never recurses back into the allocator.
// tcmalloc does the same with arena.h / metadata_allocator.h.

#pragma once

#include <cstddef>

#include "Common.hpp"

namespace PenMemory
{
namespace os
{

// Native page size (4 KiB on x86-64 Windows/Linux).  The allocator's own page
// size is PageSize (8 KiB); this is only used to talk to the kernel.
Usize GetSystemPageSize();

// Allocation granularity: VirtualAlloc rounds every reservation up to this
// (64 KiB on Windows).  On POSIX it equals the page size.
Usize AllocGranularity();

// Reserves and commits `bytes` of zeroed, read/write memory aligned to
// `alignment`, which must be a power of two no larger than 64 KiB (both
// platforms satisfy that for free: Windows because VirtualAlloc always returns
// 64 KiB-aligned memory, POSIX by over-mapping and trimming).
//
// Returns nullptr if the OS refuses the request.  `bytes` is rounded up to
// PageSize internally; pass the same value to Deallocate().
void* Allocate(Usize bytes, Usize alignment = PageSize);

// Releases the address space obtained from Allocate().
void Deallocate(void* ptr, Usize bytes);

// Drops the physical pages backing [ptr, ptr + bytes) while keeping the address
// space reserved.  The contents are lost; Commit() must be called before the
// range is used again.  Returns false if the OS refused (the caller then keeps
// the memory as it is), for example when the range spans two reservations on
// Windows.
bool Decommit(void* ptr, Usize bytes);

// Re-commits a range previously passed to Decommit().
bool Commit(void* ptr, Usize bytes);

// ---------------------------------------------------------------------------
// Metadata arena
// ---------------------------------------------------------------------------
// Bump allocator over OS chunks.  Memory is never returned: the allocator's
// metadata (page map nodes, Span objects) lives for the life of the process.
// Span and ThreadCache objects are recycled by their owners, so the arena only
// grows when new page-map nodes or new threads appear.
void* MetaDataAlloc(Usize bytes);
// Bytes obtained from the OS for metadata so far.
Usize MetaDataBytes();

}  // namespace os
}  // namespace PenMemory
