// File /Native/Memory/Test/UnitTest/test_interface.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// The four public entry points: allocation, deallocation, alignment, and the
// invariants a malloc replacement has to keep.

#include <atomic>
#include <thread>
#include <vector>

#include "MiniTest.h"
#include "../../Globals.hpp"

using namespace PenMemory;

namespace
{

	const SizeMap& Sizes() { return Globals::Get().SizeClasses; }

	// A deterministic PRNG so failures are reproducible.
	struct Rng
	{
		U64 State = 0x9e3779b97f4a7c15ull;
		U32 Next()
		{
			State ^= State << 13;
			State ^= State >> 7;
			State ^= State << 17;
			return static_cast<U32>(State >> 11);
		}
		Usize Below(Usize limit) { return Next() % limit; }
	};

	// Fills [ptr, ptr+size) with a pattern derived from `tag` and verifies it later.
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

	struct Block
	{
		void* Ptr = nullptr;
		Usize Size = 0;
		unsigned char Tag = 0;
	};

}  // namespace

int main()
{
	TEST("every size class round trips");
	for (Usize c = 1; c < NumClasses; ++c)
	{
		const Usize size = Sizes().ClassToSize(c);
		void* blocks[3];
		for (void*& block : blocks)
		{
			block = Allocate(size);
			EXPECT_MSG(block != nullptr, "class " + std::to_string(c));
			EXPECT_EQ(GetAllocatedSize(block), size);
			EXPECT(IsAlignedTo(block, Alignment));
			FillPattern(block, size, static_cast<unsigned char>(c));
		}
		for (Usize i = 0; i < 3; ++i)
		{
			EXPECT_MSG(CheckPattern(blocks[i], size, static_cast<unsigned char>(c)),
			 "class " + std::to_string(c));
			Deallocate(blocks[i]);
		}
	}
	CHECK_HEAP();

	TEST("every size from 1 to 4 KiB round trips");
	{
		constexpr Usize Count = 4096;
		std::vector<Block> blocks(Count);
		for (Usize i = 0; i < Count; ++i)
		{
			const Usize size = i + 1;
			void* ptr = Allocate(size);
			EXPECT(ptr != nullptr);
			EXPECT_GE(GetAllocatedSize(ptr), size);
			EXPECT(IsAlignedTo(ptr, Alignment));
			FillPattern(ptr, size, static_cast<unsigned char>(i));
			blocks[i] = Block{ptr, size, static_cast<unsigned char>(i)};
		}
		// Nothing was overwritten: no two live allocations overlap.
		for (const Block& block : blocks)
		{
			EXPECT(CheckPattern(block.Ptr, block.Size, block.Tag));
		}
		for (const Block& block : blocks) Deallocate(block.Ptr);
	}
	CHECK_HEAP();

	TEST("random sizes do not overlap");
	{
		Rng rng;
		std::vector<Block> blocks;
		blocks.reserve(4000);
		for (int i = 0; i < 4000; ++i)
		{
			Usize size = rng.Below(4096) + 1;
			if (i % 7 == 0) size = rng.Below(MaxSize) + 1;  // sometimes a big one
			void* ptr = Allocate(size);
			EXPECT(ptr != nullptr);
			const unsigned char tag = static_cast<unsigned char>(rng.Next());
			FillPattern(ptr, size, tag);
			blocks.push_back(Block{ptr, size, tag});
		}
		for (const Block& block : blocks)
		{
			EXPECT_MSG(CheckPattern(block.Ptr, block.Size, block.Tag),
			 "size=" + std::to_string(block.Size));
		}
		// Free in a shuffled order.
		for (Usize i = blocks.size(); i > 1; --i)
		{
			const Usize j = rng.Below(i);
			std::swap(blocks[i - 1], blocks[j]);
			Deallocate(blocks[i - 1].Ptr);
		}
		Deallocate(blocks[0].Ptr);
	}
	CHECK_HEAP();

	TEST("zero sized allocations and nullptr");
	{
		void* ptr = Allocate(0);
		EXPECT(ptr != nullptr);
		std::memset(ptr, 0xaa, 1);
		Deallocate(ptr);

		void* aligned = AlignedAllocate(64, 0);
		EXPECT(aligned != nullptr);
		EXPECT(IsAlignedTo(aligned, 64));
		AlignedDeallocate(aligned, 64);

		Deallocate(nullptr);
		AlignedDeallocate(nullptr, 64);
		EXPECT_EQ(GetAllocatedSize(nullptr), 0u);
	}
	CHECK_HEAP();

	TEST("allocations above MaxSize");
	{
		const Usize sizes[] = {MaxSize + 1, MaxSize + PageSize, 512 * 1024,
				 1024 * 1024 + 17, 8 * 1024 * 1024};
		for (Usize size : sizes)
		{
			void* ptr = Allocate(size);
			EXPECT_MSG(ptr != nullptr, "size=" + std::to_string(size));
			EXPECT_GE(GetAllocatedSize(ptr), size);
			EXPECT(IsAlignedTo(ptr, PageSize));
			FillPattern(ptr, size, 0x5c);
			EXPECT(CheckPattern(ptr, size, 0x5c));
			Deallocate(ptr);
		}
		CHECK_HEAP();
	}

	TEST("estimated sizes");
	{
		EXPECT_GE(GetEstimatedAllocatedSize(1), 1u);
		EXPECT_EQ(GetEstimatedAllocatedSize(MaxSize), MaxSize);
		EXPECT_GE(GetEstimatedAllocatedSize(MaxSize + 1), MaxSize + 1);
		EXPECT_EQ(GetEstimatedAllocatedSize(MaxSize + 1) % PageSize, 0u);
	}

	TEST("aligned allocations");
	{
		const Usize alignments[] = {8,      16,     32,     64,      128,
									256,    512,    1024,   2048,    4096,
									8192,   16384,  65536,  1024 * 1024};
		const Usize sizes[] = {1, 7, 100, 1000, 5000, 40000, 200000, 300000};
		for (Usize alignment : alignments)
		{
			for (Usize size : sizes)
			{
				void* ptr = AlignedAllocate(alignment, size);
				EXPECT_MSG(ptr != nullptr,
			   "align=" + std::to_string(alignment) +
				 " size=" + std::to_string(size));
				if (ptr == nullptr) continue;
				EXPECT_MSG(IsAlignedTo(ptr, alignment),
			   "align=" + std::to_string(alignment) +
				 " size=" + std::to_string(size));
				EXPECT_GE(GetAllocatedSize(ptr), size);
				FillPattern(ptr, size, 0x3b);
				EXPECT(CheckPattern(ptr, size, 0x3b));
				AlignedDeallocate(ptr, alignment);
			}
		}
		CHECK_HEAP();
	}

	TEST("aligned allocations do not overlap");
	{
		std::vector<Block> blocks;
		for (Usize alignment = 16; alignment <= 4096; alignment <<= 1)
		{
			void* ptr = AlignedAllocate(alignment, 3000);
			EXPECT(ptr != nullptr);
			EXPECT(IsAlignedTo(ptr, alignment));
			FillPattern(ptr, 3000, static_cast<unsigned char>(alignment));
			blocks.push_back(Block{ptr, 3000, static_cast<unsigned char>(alignment)});
		}
		for (const Block& block : blocks)
		{
			EXPECT(CheckPattern(block.Ptr, block.Size, block.Tag));
		}
		for (const Block& block : blocks) Deallocate(block.Ptr);
		CHECK_HEAP();
	}

	TEST("statistics account for the memory");
	{
		PenMemoryStats before{};
		GetStats(&before);
		EXPECT_GE(before.SystemBytes, before.PageAllocatedBytes);

		std::vector<void*> blocks;
		for (int i = 0; i < 512; ++i) blocks.push_back(Allocate(700));
		PenMemoryStats during{};
		GetStats(&during);
		EXPECT_GE(during.PageAllocatedBytes + during.CentralCacheBytes +
			during.ThreadCacheBytes,
		  before.PageAllocatedBytes + before.CentralCacheBytes +
			before.ThreadCacheBytes);
		for (void* block : blocks) Deallocate(block);
		CHECK_HEAP();
	}

	TEST("free memory is returned to the OS and reused");
	{
		std::vector<void*> blocks;
		for (int i = 0; i < 2048; ++i) blocks.push_back(Allocate(8192));
		for (void* block : blocks) Deallocate(block);
		blocks.clear();

		const Usize released = ReleaseFreeMemory();
		EXPECT_GE(released, 1u);
		PenMemoryStats releasedStats{};
		GetStats(&releasedStats);
		EXPECT_GE(releasedStats.PageReleasedBytes, PageSize);

		// Memory still has to work after being handed back.
		for (int i = 0; i < 2048; ++i)
		{
			void* block = Allocate(8192);
			EXPECT(block != nullptr);
			FillPattern(block, 8192, 0x21);
		}
		CHECK_HEAP();
	}

	TEST("allocations survive a thread that allocated them");
	{
		// Objects allocated by a worker thread must stay valid (and be freeable)
		// after that thread is gone.
		void* block = nullptr;
		std::thread worker([&block] { block = Allocate(1000); });
		worker.join();
		EXPECT(block != nullptr);
		FillPattern(block, 1000, 0x66);
		EXPECT(CheckPattern(block, 1000, 0x66));
		Deallocate(block);
		CHECK_HEAP();
	}

	TEST("cross-thread deallocation");
	{
		constexpr Usize Slots = 2000;
		std::vector<Block> blocks(Slots);

		// Phase 1: every thread fills its own quarter of the table.
		std::thread producers[4];
		for (int t = 0; t < 4; ++t)
		{
			producers[t] = std::thread([&blocks, t]
			{
				Rng rng;
				rng.State += static_cast<U64>(t) * 7919;
				for (Usize slot = static_cast<Usize>(t); slot < Slots; slot += 4)
				{
					const Usize size = rng.Below(3000) + 1;
					void* ptr = Allocate(size);
					const unsigned char tag = static_cast<unsigned char>(rng.Next());
					FillPattern(ptr, size, tag);
					blocks[slot] = Block{ptr, size, tag};
				}
			});
		}
		for (std::thread& producer : producers) producer.join();

		// Phase 2: every thread frees the quarter its neighbour produced.
		std::atomic<int> mismatches{0};
		std::thread consumers[4];
		for (int t = 0; t < 4; ++t)
		{
			consumers[t] = std::thread([&blocks, &mismatches, t]
			{
				for (Usize slot = static_cast<Usize>(t); slot < Slots; slot += 4)
				{
					const Block& block = blocks[slot];
					if (!CheckPattern(block.Ptr, block.Size, block.Tag))
					{
						mismatches.fetch_add(1);
					}
					Deallocate(block.Ptr);
				}
			});
		}
		for (std::thread& consumer : consumers) consumer.join();
		EXPECT_EQ(mismatches.load(), 0);
	}
	CHECK_HEAP();

	TEST("concurrent random workload");
	{
		std::atomic<int> failures{0};
		std::thread workers[4];
		for (int t = 0; t < 4; ++t)
		{
			workers[t] = std::thread([&failures, t]
			{
				Rng rng;
				rng.State += static_cast<U64>(t) * 104729;
				std::vector<Block> live;
				for (int step = 0; step < 20000; ++step)
				{
					const bool allocate = live.empty() || rng.Below(100) < 55;
					if (allocate)
					{
						Usize size = rng.Below(2048) + 1;
						const Usize roll = rng.Below(100);
						if (roll == 0) size = rng.Below(MaxSize * 2) + 1;   // page level
						if (roll == 1) size = rng.Below(20000) + 1;
						const Usize alignment =
							roll == 2 ? Usize{64} << rng.Below(6) : 0;
						void* ptr = alignment != 0 ? AlignedAllocate(alignment, size)
						   : Allocate(size);
						if (ptr == nullptr)
						{
							failures.fetch_add(1);
							continue;
						}
						if (alignment != 0 && !IsAlignedTo(ptr, alignment))
						{
							failures.fetch_add(1);
						}
						const unsigned char tag = static_cast<unsigned char>(rng.Next());
						FillPattern(ptr, size, tag);
						live.push_back(Block{ptr, size, tag});
					}
					else
					{
						const Usize index = rng.Below(live.size());
						const Block block = live[index];
						if (!CheckPattern(block.Ptr, block.Size, block.Tag))
						{
							failures.fetch_add(1);
						}
						Deallocate(block.Ptr);
						live[index] = live.back();
						live.pop_back();
					}
				}
				for (const Block& block : live)
				{
					if (!CheckPattern(block.Ptr, block.Size, block.Tag))
					{
						failures.fetch_add(1);
					}
					Deallocate(block.Ptr);
				}
			});
		}
		for (std::thread& worker : workers) worker.join();
		EXPECT_EQ(failures.load(), 0);
	}
	CHECK_HEAP();

	return PenMemoryTest::Summary("interface");
}
