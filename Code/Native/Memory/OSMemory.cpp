// File /Native/Memory/OSMemory.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: OS memory primitives (implementation).

#include "OSMemory.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace PenMemory
{

void CrashWithMessage(const char* file, int line, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	std::fprintf(stderr, "[PenMemory] %s:%d: ", file, line);
	std::vfprintf(stderr, fmt, args);
	std::fprintf(stderr, "\n");
	va_end(args);
	std::fflush(stderr);
	std::abort();
}

void ReportCorruptedFree(const void* ptr, const char* what)
{
	CrashWithMessage(__FILE__, __LINE__,
				   "heap corruption detected: %s (ptr=%p). "
				   "Double free, wild pointer, or a pointer not owned by "
				   "PenMemory?",
				   what, ptr);
}

namespace os
{
namespace
{

#if defined(_WIN32)
Usize SystemPageSize()
{
	static const Usize value = []
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		return static_cast<Usize>(info.dwPageSize);
	}();
	return value;
}
Usize SystemAllocGranularity()
{
	static const Usize value = []
	{
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		return static_cast<Usize>(info.dwAllocationGranularity);
	}();
	return value;
}
#else
Usize SystemPageSize()
{
	static const Usize value = static_cast<Usize>(sysconf(_SC_PAGESIZE));
	return value;
}
Usize SystemAllocGranularity() { return SystemPageSize(); }
#endif

// All sizes handed to the OS are rounded to the allocator's page size, which is
// always a multiple of the native page size on the platforms we support.
inline Usize RoundUpToPage(Usize bytes)
{
	return (bytes + PageSize - 1) & ~(PageSize - 1);
}

// Largest alignment Allocate() can satisfy by itself.
inline constexpr Usize MaxSupportedAlignment = 64 * 1024;

}  // namespace

Usize GetSystemPageSize() { return SystemPageSize(); }
Usize AllocGranularity() { return SystemAllocGranularity(); }

void* Allocate(Usize bytes, Usize alignment)
{
	PEN_MEMORY_CHECK(alignment != 0 && (alignment & (alignment - 1)) == 0);
	if (bytes == 0) bytes = PageSize;
	const Usize rounded = RoundUpToPage(bytes);

#if defined(_WIN32)
	// VirtualAlloc always returns an address that is a multiple of the
	// allocation granularity (64 KiB on every supported Windows version), which
	// covers every alignment we ask for here.  Larger alignments are handled by
	// PageCache::NewAligned(), which carves an aligned span out of a larger one.
	if (alignment > SystemAllocGranularity()) return nullptr;
	void* p = VirtualAlloc(nullptr, rounded, MEM_RESERVE | MEM_COMMIT,
					   PAGE_READWRITE);
	return p;
#else
	const Usize align = std::max({alignment, PageSize, GetSystemPageSize()});
	// Over-map so that we can trim to the requested alignment.
	const Usize request = rounded + align;
	void* base = mmap(nullptr, request, PROT_READ | PROT_WRITE,
										MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) return nullptr;

	const Usize raw = reinterpret_cast<Usize>(base);
	const Usize start = (raw + align - 1) & ~(align - 1);
	const Usize head = static_cast<Usize>(start - raw);
	if (head != 0)
	{
		// Both the head and the tail are whole pages, so they can be trimmed.
		munmap(base, head);
	}
	const Usize tail = request - head - rounded;
	if (tail != 0)
	{
		munmap(reinterpret_cast<void*>(start + rounded), tail);
	}
	return reinterpret_cast<void*>(start);
#endif
}

void Deallocate(void* ptr, Usize bytes)
{
	if (ptr == nullptr) return;
	const Usize rounded = RoundUpToPage(bytes);
#if defined(_WIN32)
	(void)rounded;
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	(void)munmap(ptr, rounded);
#endif
}

bool Decommit(void* ptr, Usize bytes)
{
	if (ptr == nullptr || bytes == 0) return true;
	const Usize rounded = RoundUpToPage(bytes);
#if defined(_WIN32)
	// MEM_DECOMMIT only works inside a single reservation, so callers must keep
	// spans from being merged across the boundaries of our growth chunks.
	return VirtualFree(ptr, rounded, MEM_DECOMMIT) != 0;
#else
	return madvise(ptr, rounded, MADV_DONTNEED) == 0;
#endif
}

bool Commit(void* ptr, Usize bytes)
{
	if (ptr == nullptr || bytes == 0) return true;
	const Usize rounded = RoundUpToPage(bytes);
#if defined(_WIN32)
	return VirtualAlloc(ptr, rounded, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
	// Anonymous mappings stay committed on POSIX; the pages just have to be
	// faulted back in on first touch.
	(void)rounded;
	return true;
#endif
}

// ---------------------------------------------------------------------------
// Metadata arena
// ---------------------------------------------------------------------------
namespace
{

inline constexpr Usize MetadataChunkSize = Usize{1} << 20;  // 1 MiB

class MetadataArena
{
 public:
	void* Allocate(Usize bytes, Usize align = Alignment)
	{
		if (bytes == 0) bytes = 1;
		SpinLockHolder h(&m_lock);
		Usize current = reinterpret_cast<Usize>(m_ptr);
		Usize aligned = (current + align - 1) & ~(align - 1);
		if (aligned + bytes > reinterpret_cast<Usize>(m_end))
		{
			if (!Grow(bytes + align)) return nullptr;
			current = reinterpret_cast<Usize>(m_ptr);
			aligned = (current + align - 1) & ~(align - 1);
		}
		m_ptr = reinterpret_cast<char*>(aligned + bytes);
		m_bytesUsed += bytes;
		return reinterpret_cast<void*>(aligned);
	}

	Usize BytesUsed() const { return m_bytesUsed; }

 private:
	struct Chunk
	{
		Chunk* next;
		Usize size;
	};

	bool Grow(Usize need)
	{
		Usize size = std::max(MetadataChunkSize, need + sizeof(Chunk));
		size = (size + PageSize - 1) & ~(PageSize - 1);
		void* mem = os::Allocate(size, PageSize);
		if (mem == nullptr) return false;
		Chunk* chunk = static_cast<Chunk*>(mem);
		chunk->next = m_chunks;
		chunk->size = size;
		m_chunks = chunk;
		m_ptr = reinterpret_cast<char*>(chunk + 1);
		m_end = reinterpret_cast<char*>(chunk) + size;
		m_bytesFromOs += size;
		return true;
	}

	SpinLock m_lock;
	char* m_ptr = nullptr;
	char* m_end = nullptr;
	Chunk* m_chunks = nullptr;
	Usize m_bytesUsed = 0;
	Usize m_bytesFromOs = 0;
};

MetadataArena& Arena()
{
	// Namespace-scope object with constant initialization: no dynamic initializer,
	// no destruction order concerns.  Metadata must outlive everything.
	static MetadataArena arena;
	return arena;
}

}  // namespace

void* MetaDataAlloc(Usize bytes) { return Arena().Allocate(bytes); }
Usize MetaDataBytes() { return Arena().BytesUsed(); }

}  // namespace os
}  // namespace PenMemory
