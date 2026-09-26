// File /Native/Memory/Test/UnitTest/test_pagecache.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Page cache: carving, coalescing, alignment, release to the OS.

#include "MiniTest.h"
#include "PageCache.h"
#include "../../Globals.hpp"

using namespace PenMemory;

namespace
{

	PageCache& PC() { return Globals::Get().Pages; }

	bool AllPagesMapTo(Span* span)
	{
		PageMap& pageMap = Globals::Get().PageMaps;
		for (PageId p = span->FirstPage(); p <= span->LastPage(); p = p + Length(1))
		{
			if (pageMap.Get(p) != span) return false;
		}
		return true;
	}

	// Touches every page so that a decommitted (or freshly committed) range is
	// actually faulted in and writable.
	void FillPages(Span* span, unsigned char byte)
	{
		unsigned char* p = static_cast<unsigned char*>(span->StartAddress());
		for (Usize offset = 0; offset < span->BytesInSpan(); offset += PageSize)
		{
			p[offset] = byte;
		}
	}

}  // namespace

int main()
{
	// Automatic release would make the accounting assertions below racy.
	PC().SetReleaseThreshold(Length(0));
	CHECK_HEAP();

	TEST("a fresh page cache owns nothing");
	{
		PageCache::Stats stats = PC().GetStats();
		EXPECT_EQ(stats.SystemBytes, 0u);
		EXPECT_EQ(stats.FreeBytes, 0u);
		EXPECT_EQ(stats.FreeSpans, 0u);
	}

	TEST("New/Delete a single page");
	PageId first;
	{
		Span* span = PC().New(Length(1));
		EXPECT(span != nullptr);
		EXPECT_EQ(span->NumPages().RawNum(), 1u);
		EXPECT(span->IsAllocated());
		EXPECT_EQ(span->SizeClass(), 0u);
		EXPECT(IsAlignedTo(span->StartAddress(), PageSize));
		EXPECT_EQ(span->BytesInSpan(), PageSize);
		EXPECT(AllPagesMapTo(span));
		first = span->FirstPage();

		PageCache::Stats stats = PC().GetStats();
		EXPECT_EQ(stats.SystemBytes, MinSystemAlloc);  // one chunk
		EXPECT_EQ(stats.AllocatedBytes, PageSize);
		EXPECT_EQ(stats.FreeBytes, MinSystemAlloc - PageSize);

		EXPECT(Globals::Get().PageMaps.Get(first) == span);
		FillPages(span, 0xab);
		PC().Delete(span);
		EXPECT_EQ(Globals::Get().PageMaps.Get(first)->State(), SpanState::Free);
	}
	CHECK_HEAP();

	TEST("a returned page is reused instead of growing the heap");
	{
		const Usize systemBefore = PC().GetStats().SystemBytes;
		Span* span = PC().New(Length(1));
		EXPECT(span != nullptr);
		EXPECT_EQ(span->FirstPage().Index(), first.Index());
		EXPECT_EQ(PC().GetStats().SystemBytes, systemBefore);
		PC().Delete(span);
	}
	CHECK_HEAP();

	TEST("free neighbours coalesce");
	{
		Span* spans[8];
		for (Span*& span : spans)
		{
			span = PC().New(Length(1));
			EXPECT(span != nullptr);
		}
		const PageCache::Stats during = PC().GetStats();
		EXPECT_EQ(during.AllocatedBytes, 8 * PageSize);
		bool distinct = true;
		for (int i = 0; i < 8; ++i)
		{
			for (int j = 0; j < i; ++j) distinct = distinct && spans[i] != spans[j];
		}
		EXPECT(distinct);

		for (Span* span : spans) PC().Delete(span);

		// Everything merged back into a single span covering the whole chunk.
		const PageCache::Stats after = PC().GetStats();
		EXPECT_EQ(after.AllocatedBytes, 0u);
		EXPECT_EQ(after.FreeBytes, after.SystemBytes);
		EXPECT_EQ(after.FreeSpans, 1u);
	}
	CHECK_HEAP();

	TEST("New(n) is contiguous and page aligned");
	{
		const Length n(37);
		Span* span = PC().New(n);
		EXPECT(span != nullptr);
		EXPECT_EQ(span->NumPages().RawNum(), n.RawNum());
		EXPECT_EQ((span->LastPage() - span->FirstPage() + Length(1)).RawNum(),
		  n.RawNum());
		EXPECT_EQ(span->BytesInSpan(), n.InBytes());
		EXPECT(IsAlignedTo(span->StartAddress(), PageSize));
		EXPECT(AllPagesMapTo(span));
		FillPages(span, 0x5a);
		PC().Delete(span);
	}
	CHECK_HEAP();

	TEST("NewAligned returns an aligned span");
	{
		for (Usize alignPages : {Usize{2}, Usize{4}, Usize{16}, Usize{64}, Usize{512}})
		{
			Span* span = PC().NewAligned(Length(3), Length(alignPages));
			EXPECT_MSG(span != nullptr, "alignPages=" + std::to_string(alignPages));
			if (span == nullptr) continue;
			EXPECT_EQ(span->FirstPage().Index() % alignPages, 0u);
			EXPECT_GE(span->NumPages().RawNum(), 3u);
			EXPECT(IsAlignedTo(span->StartAddress(), alignPages * PageSize));
			EXPECT(AllPagesMapTo(span));
			FillPages(span, 0x11);
			PC().Delete(span);
		}
	}
	CHECK_HEAP();

	TEST("large allocations span many chunks");
	{
		const Length n(4096);  // 32 MiB, more than one 1 MiB growth chunk
		Span* span = PC().New(n);
		EXPECT(span != nullptr);
		EXPECT_EQ(span->NumPages().RawNum(), n.RawNum());
		EXPECT(AllPagesMapTo(span));
		FillPages(span, 0x77);
		PC().Delete(span);
	}
	CHECK_HEAP();

	TEST("memory can be released to and reclaimed from the OS");
	{
		PageCache::Stats before = PC().GetStats();
		EXPECT_EQ(before.AllocatedBytes, 0u);
		EXPECT_GE(before.FreeBytes, 2048u * PageSize);

		const Length released = PC().ReleaseAtLeastNPages(Length(2048));
		EXPECT_GE(released.RawNum(), 2048u);

		PageCache::Stats releasedStats = PC().GetStats();
		EXPECT_GE(releasedStats.ReleasedBytes, 2048u * PageSize);
		EXPECT_EQ(releasedStats.FreeBytes + releasedStats.ReleasedBytes,
		  before.FreeBytes);
		CHECK_HEAP();

		// Reuse triggers a re-commit and must still hand out writable memory.
		Span* span = PC().New(Length(2048));
		EXPECT(span != nullptr);
		FillPages(span, 0x33);
		EXPECT(AllPagesMapTo(span));
		PC().Delete(span);

		PageCache::Stats revived = PC().GetStats();
		EXPECT(releasedStats.ReleasedBytes > revived.ReleasedBytes);
		CHECK_HEAP();
	}

	TEST("released spans are not handed out again without a commit");
	{
		// Release everything, then ask for a lot of memory: the page cache has to
		// revive released spans (or grow) and everything must still be usable.
		PC().ReleaseAtLeastNPages(Length(1u << 30));
		const PageCache::Stats stats = PC().GetStats();
		EXPECT_EQ(stats.FreeBytes, 0u);
		EXPECT_GE(stats.ReleasedBytes, PageSize);

		Span* spans[16];
		for (Span*& span : spans)
		{
			span = PC().New(Length(1));
			EXPECT(span != nullptr);
			FillPages(span, 0x99);
		}
		for (Span* span : spans) PC().Delete(span);
		CHECK_HEAP();
	}

	TEST("span object free lists");
	{
		Span* span = PC().New(Length(1));
		EXPECT(span != nullptr);
		const Usize objectSize = 64;
		span->BuildFreelist(objectSize);
		EXPECT_EQ(span->NumObjects(), PageSize / objectSize);
		EXPECT_EQ(span->NumAllocated(), 0u);

		void* batch[32];
		const int got = span->PopBatch(batch, 32, objectSize);
		EXPECT_EQ(got, 32);
		EXPECT_EQ(span->NumAllocated(), 32u);
		char* base = static_cast<char*>(span->StartAddress());
		for (int i = 0; i < got; ++i)
		{
			EXPECT_EQ(batch[i], base + static_cast<Usize>(i) * objectSize);
			for (int j = 0; j < i; ++j) EXPECT(batch[i] != batch[j]);
		}

		for (int i = 0; i < got; ++i) span->Push(batch[i], objectSize);
		EXPECT_EQ(span->NumAllocated(), 0u);
		EXPECT_EQ(span->PopBatch(batch, 32, objectSize), 32);
		for (int i = 0; i < 32; ++i) span->Push(batch[i], objectSize);

		PC().Delete(span);
		CHECK_HEAP();
	}

	TEST("page map covers a whole leaf");
	{
		// A leaf covers 32 MiB of address space; make sure the radix tree can index
		// the whole range without gaps.
		const Usize leafPages = PageMap::LeafCoveredBytes() / PageSize;
		Span* span = PC().New(Length(leafPages + 1));
		EXPECT(span != nullptr);
		EXPECT(AllPagesMapTo(span));
		PC().Delete(span);
		CHECK_HEAP();
	}

	return PenMemoryTest::Summary("page cache");
}
