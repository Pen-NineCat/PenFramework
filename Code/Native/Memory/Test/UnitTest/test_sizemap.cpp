// File /Native/Memory/Test/UnitTest/test_sizemap.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Size class table and size -> class mapping.

#include "Common.hpp"
#include "MiniTest.h"
#include "../../Globals.hpp"

using namespace PenMemory;

int main()
{
	const SizeMap& sizeMap = Globals::Get().SizeClasses;

	TEST("table consistency");
	// Classes are strictly increasing, page aligned spans, at least one object
	// per span, sane batch sizes.
	for (Usize c = 1; c < NumClasses; ++c)
	{
		EXPECT(SizeClassTable[c].Size > SizeClassTable[c - 1].Size);
		EXPECT_EQ(SizeClassTable[c].SpanBytes % PageSize, 0u);
		EXPECT_GE(SizeClassTable[c].SpanBytes / SizeClassTable[c].Size, 1u);
		EXPECT_GE(SizeClassTable[c].Batch, 2u);
		EXPECT(SizeClassTable[c].Batch <= MaxObjectsToMove);
		EXPECT_EQ(sizeMap.ClassToSize(c), SizeClassTable[c].Size);
		EXPECT_EQ(sizeMap.ClassToPages(c), SizeClassTable[c].SpanBytes / PageSize);
		EXPECT_EQ(sizeMap.NumObjectsToMove(c), SizeClassTable[c].Batch);
		EXPECT_EQ(sizeMap.ObjectsPerSpan(c),
		  SizeClassTable[c].SpanBytes / SizeClassTable[c].Size);
	}
	EXPECT_EQ(SizeClassTable[NumClasses - 1].Size, MaxSize);
	EXPECT_EQ(sizeMap.ClassToSize(0), 0u);

	TEST("size -> class is the smallest fitting class");
	for (Usize c = 1; c < NumClasses; ++c)
	{
		const Usize size = sizeMap.ClassToSize(c);
		EXPECT_EQ(sizeMap.SizeClass(size), c);
		if (c > 1)
		{
			// One byte less must not need this class...
			const Usize smaller = sizeMap.ClassToSize(c - 1);
			EXPECT(sizeMap.SizeClass(smaller) < c);
			// ... and every size in between maps to this class.
			EXPECT_EQ(sizeMap.SizeClass(smaller + 1), c);
			if (size > smaller + 1) EXPECT_EQ(sizeMap.SizeClass(size - 1), c);
		}
	}
	EXPECT_EQ(sizeMap.SizeClass(1), 1u);
	EXPECT_EQ(sizeMap.SizeClass(MaxSize), NumClasses - 1);
	EXPECT_EQ(sizeMap.SizeClass(MaxSize + 1), 0u);
	EXPECT_EQ(sizeMap.SizeClass(MaxSize * 4), 0u);

	TEST("covers every size in [1, MaxSize]");
	for (Usize size = 1; size <= MaxSize; ++size)
	{
		const Usize c = sizeMap.SizeClass(size);
		EXPECT_MSG(c >= 1 && c < NumClasses, "size=" + std::to_string(size));
		EXPECT_MSG(sizeMap.ClassToSize(c) >= size,
		   "size=" + std::to_string(size));
		EXPECT_MSG(c == 1 || sizeMap.ClassToSize(c - 1) < size,
		   "size=" + std::to_string(size));
	}

	TEST("aligned size class lookup");
	for (Usize alignment = Alignment; alignment <= PageSize; alignment <<= 1)
	{
		for (Usize size = 1; size <= MaxSize; size += 977)
		{
			const Usize c = sizeMap.SizeClassAligned(size, alignment);
			EXPECT_MSG(c >= 1 && c < NumClasses,
			 "size=" + std::to_string(size) + " align=" +
			   std::to_string(alignment));
			EXPECT_EQ(sizeMap.ClassToSize(c) % alignment, 0u);
			EXPECT_GE(sizeMap.ClassToSize(c), size);
		}
		// Boundaries.
		EXPECT_EQ(sizeMap.ClassToSize(sizeMap.SizeClassAligned(1, alignment)) %
			alignment,
		  0u);
		EXPECT_EQ(sizeMap.ClassToSize(sizeMap.SizeClassAligned(MaxSize, alignment)) %
			alignment,
		  0u);
	}

	TEST("size class sizes are 16-byte aligned above the 8-byte class");
	for (Usize c = 2; c < NumClasses; ++c)
	{
		EXPECT_EQ(sizeMap.ClassToSize(c) % alignof(std::max_align_t), 0u);
	}

	TEST("page arithmetic");
	EXPECT_EQ(BytesToLengthCeil(1).RawNum(), 1u);
	EXPECT_EQ(BytesToLengthCeil(PageSize).RawNum(), 1u);
	EXPECT_EQ(BytesToLengthCeil(PageSize + 1).RawNum(), 2u);
	EXPECT_EQ(BytesToLengthFloor(PageSize - 1).RawNum(), 0u);
	EXPECT_EQ(BytesToLengthFloor(MaxSize).RawNum(), MaxSize / PageSize);
	EXPECT_EQ(PageIdContaining(reinterpret_cast<void*>(0x400000)).Index(),
		0x400000u >> PageShift);

	return PenMemoryTest::Summary("size classes");
}
