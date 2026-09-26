// File /Native/Memory/Test/UnitTest/test_centralcache.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Central cache: batch removal, span lifecycle, size class attribution.

#include "MiniTest.h"
#include "CentralCache.h"
#include "../../Globals.hpp"

using namespace PenMemory;

namespace
{

	CentralCache& CC() { return Globals::Get().Central; }
	PageCache& PC() { return Globals::Get().Pages; }
	const SizeMap& Sizes() { return Globals::Get().SizeClasses; }

}  // namespace

int main()
{
	PC().SetReleaseThreshold(Length(0));
	const Usize small = 13;  // 256-byte objects, one page per span, 32 objects
	const Usize big = 45;    // 131072-byte objects, 16 pages per span, 1 object

	EXPECT_EQ(Sizes().ClassToSize(small), 256u);
	EXPECT_EQ(Sizes().ObjectsPerSpan(small), 32u);
	EXPECT_EQ(Sizes().ObjectsPerSpan(big), 1u);

	TEST("RemoveRange hands out distinct objects of one span");
	{
		void* batch[16];
		const int got = CC().RemoveRange(small, batch, 16);
		EXPECT_EQ(got, 16);
		for (int i = 0; i < got; ++i)
		{
			EXPECT(IsAlignedTo(batch[i], Alignment));
			EXPECT(PageIdContaining(batch[i]) == PageIdContaining(batch[0]));
			EXPECT_EQ(Globals::Get().PageMaps.Get(PageIdContaining(batch[i]))->SizeClass(),
			small);
			for (int j = 0; j < i; ++j) EXPECT(batch[i] != batch[j]);
			std::memset(batch[i], 0x5a, Sizes().ClassToSize(small));
		}
		// 32 objects exist, 16 are out, 16 are still cached.
		EXPECT_EQ(CC().FreeListOf(small).CachedObjectCount(), 16u);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 1u);
		CHECK_HEAP();

		// Giving half of them back leaves the span partially full...
		CC().InsertRange(small, batch, got / 2);
		EXPECT_EQ(CC().FreeListOf(small).CachedObjectCount(), 24u);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 1u);
		CHECK_HEAP();

		// ... and the rest returns the span to the page cache.
		CC().InsertRange(small, batch + got / 2, got - got / 2);
		EXPECT_EQ(CC().FreeListOf(small).CachedObjectCount(), 0u);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 0u);
		CHECK_HEAP();
	}

	TEST("a completely free span goes back to the page cache");
	{
		void* batch[32];
		EXPECT_EQ(CC().RemoveRange(small, batch, 32), 32);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 1u);
		EXPECT_EQ(CC().FreeListOf(small).CachedObjectCount(), 0u);
		CHECK_HEAP();

		CC().InsertRange(small, batch, 32);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 0u);
		EXPECT_EQ(CC().FreeListOf(small).CachedObjectCount(), 0u);
		CHECK_HEAP();
	}

	TEST("large batches are split over several spans");
	{
		// The central cache moves at most MaxObjectsToMove objects per call, which
		// is the batch size the thread cache uses.
		void* batch[100];
		int got = 0;
		while (got < 100)
		{
			const int chunk = static_cast<int>(
				std::min<Usize>(MaxObjectsToMove, 100 - static_cast<Usize>(got)));
			const int n = CC().RemoveRange(small, batch + got, chunk);
			EXPECT(n > 0);
			got += n;
		}
		EXPECT_EQ(got, 100);
		// 100 objects / 32 per span -> 4 spans (the last one is only partly used).
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 4u);
		for (int i = 0; i < 100; ++i)
		{
			std::memset(batch[i], i & 0xff, Sizes().ClassToSize(small));
		}
		for (int i = 0; i < 100; i += 32)
		{
			const int n = std::min(32, 100 - i);
			CC().InsertRange(small, batch + i, n);
		}
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 0u);
		CHECK_HEAP();
	}

	TEST("partially drained spans are reused before new spans are allocated");
	{
		void* first[8];
		EXPECT_EQ(CC().RemoveRange(small, first, 8), 8);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 1u);
		CC().InsertRange(small, first, 8);

		void* second[8];
		EXPECT_EQ(CC().RemoveRange(small, second, 8), 8);
		EXPECT_EQ(CC().FreeListOf(small).NumSpans(), 1u);
		// Same span, so the objects come from the same page.
		EXPECT(PageIdContaining(second[0]) == PageIdContaining(first[0]));
		CC().InsertRange(small, second, 8);
		CHECK_HEAP();
	}

	TEST("spans with a single object");
	{
		void* objects[4];
		for (void*& object : objects)
		{
			// A batch of N is capped at the one object the span holds.
			EXPECT_EQ(CC().RemoveRange(big, &object, 2), 1);
			EXPECT(IsAlignedTo(object, PageSize));
			std::memset(object, 0x7f, Sizes().ClassToSize(big));
		}
		EXPECT_EQ(CC().FreeListOf(big).NumSpans(), 4u);
		EXPECT_EQ(CC().FreeListOf(big).CachedObjectCount(), 0u);
		CHECK_HEAP();

		for (void* object : objects) CC().InsertRange(big, &object, 1);
		EXPECT_EQ(CC().FreeListOf(big).NumSpans(), 0u);
		CHECK_HEAP();
	}

	TEST("every size class can be filled and drained");
	{
		for (Usize c = 1; c < NumClasses; ++c)
		{
			const int count = 5;
			void* batch[8];
			int got = 0;
			while (got < count)
			{
				const int n = CC().RemoveRange(c, batch + got, count - got);
				if (n == 0) break;
				got += n;
			}
			EXPECT_MSG(got == count, "size class " + std::to_string(c));
			for (int i = 0; i < got; ++i)
			{
				std::memset(batch[i], 0xcd, Sizes().ClassToSize(c));
			}
			CC().InsertRange(c, batch, got);
			EXPECT_MSG(CC().FreeListOf(c).NumSpans() == 0,
			 "size class " + std::to_string(c) + " did not release its span");
		}
		CHECK_HEAP();
	}

	TEST("objects belong to the span the page map reports");
	{
		void* objects[32];
		EXPECT_EQ(CC().RemoveRange(small, objects, 32), 32);
		PageMap& pageMap = Globals::Get().PageMaps;
		for (void* object : objects)
		{
			Span* span = pageMap.Get(PageIdContaining(object));
			EXPECT(span != nullptr);
			EXPECT_EQ(span->SizeClass(), small);
			const Usize offset = reinterpret_cast<Usize>(object) -
				 reinterpret_cast<Usize>(span->StartAddress());
			EXPECT_LT(offset, span->BytesInSpan());
		}
		CC().InsertRange(small, objects, 32);
		CHECK_HEAP();
	}

	TEST("central cache accounting");
	{
		EXPECT_EQ(CC().CachedBytes(), 0u);
		EXPECT_EQ(CC().LiveSpans(), 0u);
		void* objects[33];
		EXPECT_EQ(CC().RemoveRange(small, objects, 32), 32);
		EXPECT_EQ(CC().RemoveRange(small, objects + 32, 1), 1);
		EXPECT_EQ(CC().LiveSpans(), 2u);
		// 2 spans of 32 objects, 33 handed out -> 31 still cached.
		EXPECT_EQ(CC().CachedBytes(), 31u * 256u);
		CC().InsertRange(small, objects, 32);
		CC().InsertRange(small, objects + 32, 1);
		EXPECT_EQ(CC().CachedBytes(), 0u);
		EXPECT_EQ(CC().LiveSpans(), 0u);
		CHECK_HEAP();
	}

	return PenMemoryTest::Summary("central cache");
}
