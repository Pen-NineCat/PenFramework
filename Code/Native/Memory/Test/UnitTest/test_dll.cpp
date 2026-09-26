// File /Native/Memory/Test/UnitTest/test_dll.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// The exported C ABI: everything a client of PenMemory.dll can reach.
//
// Unlike test_interface (which compiles the allocator sources and therefore sees
// the internal C++ types), this binary links the DLL and only uses Interface.h.
// It is the check that the exported surface really works through the DLL
// boundary, including the 64 bit statistics struct.

#include <thread>
#include <vector>

#include "MiniTest.h"

using namespace PenMemory;

namespace
{
	struct Block
	{
		void* Ptr = nullptr;
		Usize Size = 0;
		unsigned char Tag = 0;
	};

	void FillPattern(void* ptr, Usize size, unsigned char tag)
	{
		unsigned char* bytes = static_cast<unsigned char*>(ptr);
		for (Usize i = 0; i < size; ++i)
		{
			bytes[i] = static_cast<unsigned char>(tag + i * 31);
		}
	}

	bool CheckPattern(const void* ptr, Usize size, unsigned char tag)
	{
		const unsigned char* bytes = static_cast<const unsigned char*>(ptr);
		for (Usize i = 0; i < size; ++i)
		{
			if (bytes[i] != static_cast<unsigned char>(tag + i * 31)) return false;
		}
		return true;
	}
}

int main()
{
	TEST("basic round trip");
	{
		const Usize sizes[] = {1, 8, 7, 64, 1024, 4096, 65536, 200000, 1024 * 1024};
		for (Usize size : sizes)
		{
			void* ptr = Allocate(size);
			EXPECT_MSG(ptr != nullptr, "size=" + std::to_string(size));
			if (ptr == nullptr) continue;
			EXPECT_GE(GetAllocatedSize(ptr), size);
			FillPattern(ptr, size, 0x5a);
			EXPECT(CheckPattern(ptr, size, 0x5a));
			Deallocate(ptr);
		}

		void* zero = Allocate(0);
		EXPECT(zero != nullptr);
		Deallocate(zero);
		Deallocate(nullptr);
	}
	CHECK_HEAP();

	TEST("reported geometry");
	{
		EXPECT_EQ(GetPageSize(), static_cast<Usize>(8192));
		EXPECT_EQ(GetAlignment(), static_cast<Usize>(PEN_MEMORY_BASIC_ALIGNMENT));
		EXPECT_EQ(GetEstimatedAllocatedSize(1), static_cast<Usize>(8));
		// Above the size-class range every request is rounded up to whole pages.
		EXPECT_EQ(GetEstimatedAllocatedSize(1024 * 1024 + 1) % GetPageSize(), static_cast<Usize>(0));
	}

	TEST("aligned allocations");
	{
		for (Usize alignment = GetAlignment(); alignment <= 4096; alignment <<= 1)
		{
			for (Usize size : {Usize{1}, Usize{1000}, Usize{5000}, Usize{300000}})
			{
				void* ptr = AlignedAllocate(alignment, size);
				EXPECT_MSG(ptr != nullptr,
			   "align=" + std::to_string(alignment) + " size=" + std::to_string(size));
				if (ptr == nullptr) continue;
				EXPECT_MSG(IsAlignedTo(ptr, alignment),
			   "align=" + std::to_string(alignment) + " size=" + std::to_string(size));
				FillPattern(ptr, size, 0x3b);
				EXPECT(CheckPattern(ptr, size, 0x3b));
				AlignedDeallocate(ptr, alignment);
			}
		}

		// Not a power of two: has to be refused, not mis-aligned.
		EXPECT(AlignedAllocate(24, 128) == nullptr);
		AlignedDeallocate(nullptr, 64);
	}
	CHECK_HEAP();

	TEST("statistics snapshot");
	{
		GetStats(nullptr);  // must be harmless

		PenMemoryStats before{};
		GetStats(&before);

		std::vector<void*> blocks;
		for (int i = 0; i < 512; ++i) blocks.push_back(Allocate(700));

		PenMemoryStats during{};
		GetStats(&during);
		EXPECT_GE(during.PageAllocatedBytes + during.CentralCacheBytes + during.ThreadCacheBytes,
		  before.PageAllocatedBytes + before.CentralCacheBytes + before.ThreadCacheBytes);
		EXPECT_GE(during.SystemBytes, during.PageAllocatedBytes);

		for (void* block : blocks) Deallocate(block);
	}
	CHECK_HEAP();

	TEST("free memory is returned to the OS and reused");
	{
		std::vector<void*> blocks;
		for (int i = 0; i < 1024; ++i) blocks.push_back(Allocate(8192));
		for (void* block : blocks) Deallocate(block);
		blocks.clear();

		EXPECT_GE(ReleaseFreeMemory(), static_cast<Usize>(1));

		PenMemoryStats stats{};
		GetStats(&stats);
		EXPECT_GE(stats.PageReleasedBytes, GetPageSize());

		for (int i = 0; i < 1024; ++i)
		{
			void* block = Allocate(8192);
			EXPECT(block != nullptr);
			FillPattern(block, 8192, 0x21);
		}
	}
	CHECK_HEAP();

	TEST("cross thread deallocation through the ABI");
	{
		constexpr Usize Slots = 512;
		std::vector<Block> blocks(Slots);

		std::thread producer([&blocks]
		{
			for (Usize i = 0; i < Slots; ++i)
			{
				const Usize size = (i % 300) + 1;
				void* ptr = Allocate(size);
				FillPattern(ptr, size, static_cast<unsigned char>(i));
				blocks[i] = Block{ptr, size, static_cast<unsigned char>(i)};
			}
		});
		producer.join();

		for (const Block& block : blocks)
		{
			EXPECT(CheckPattern(block.Ptr, block.Size, block.Tag));
		}

		std::thread consumer([&blocks]
		{
			for (const Block& block : blocks) Deallocate(block.Ptr);
		});
		consumer.join();
	}
	CHECK_HEAP();

	return PenMemoryTest::Summary("dll");
}
