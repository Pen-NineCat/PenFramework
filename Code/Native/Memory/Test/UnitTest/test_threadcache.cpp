// File /Native/Memory/Test/UnitTest/test_threadcache.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Thread cache: per-thread free lists, budget, scavenging, thread exit.

#include <atomic>
#include <thread>

#include "MiniTest.h"
#include "ThreadCache.h"
#include "../../Globals.hpp"

using namespace PenMemory;

namespace
{

	const Usize Small = 13;  // 256-byte objects

	void Touch(void* ptr, Usize size, unsigned char value)
	{
		std::memset(ptr, value, size);
	}

}  // namespace

int main()
{
	Globals::Get().Pages.SetReleaseThreshold(Length(0));
	const SizeMap& sizeMap = Globals::Get().SizeClasses;

	TEST("a cache is created on first use");
	EXPECT(ThreadCache::GetCacheIfPresent() == nullptr);
	ThreadCache* cache = ThreadCache::GetCache();
	EXPECT(cache != nullptr);
	EXPECT(ThreadCache::GetCacheIfPresent() == cache);
	EXPECT(ThreadCache::GetCache() == cache);
	// The initial budget is one StealAmount slice, like tcmalloc.
	EXPECT_GE(cache->MaxSize(), StealAmount);
	EXPECT_LE(cache->MaxSize(), MaxThreadCacheSize);
	EXPECT_EQ(ThreadCache::GetStats().CacheCount, 1u);
	CHECK_HEAP();

	TEST("allocation and deallocation go through the cache");
	{
		const Usize objectSize = sizeMap.ClassToSize(Small);
		void* objects[1000];
		for (void*& object : objects)
		{
			object = cache->Allocate(Small);
			EXPECT(object != nullptr);
			Touch(object, objectSize, 0x11);
		}
		for (void* object : objects) cache->Deallocate(object, Small);

		// Objects this thread does not need any more sit in its own free list.
		const ThreadCache::Stats stats = ThreadCache::GetStats();
		EXPECT_GE(stats.Objects[Small], 1u);
		EXPECT_EQ(stats.CacheCount, 1u);
		CHECK_HEAP();

		void* again[1000];
		for (void*& object : again) object = cache->Allocate(Small);
		for (void* object : again)
		{
			EXPECT(object != nullptr);
			cache->Deallocate(object, Small);
		}
		CHECK_HEAP();
	}

	TEST("the cache stays inside its budget");
	{
		for (int round = 0; round < 200; ++round)
		{
			void* objects[64];
			for (void*& object : objects) object = cache->Allocate(Small);
			for (void* object : objects) cache->Deallocate(object, Small);
		}
		EXPECT(cache->Size() <= cache->MaxSize() + sizeMap.ClassToSize(Small));
		EXPECT_LE(cache->MaxSize(), MaxThreadCacheSize);
		CHECK_HEAP();
	}

	TEST("many size classes can be cycled at once");
	{
		for (Usize c = 1; c < NumClasses; ++c)
		{
			const Usize objectSize = sizeMap.ClassToSize(c);
			void* objects[64];
			int got = 0;
			for (void*& object : objects)
			{
				object = cache->Allocate(c);
				if (object == nullptr) break;
				Touch(object, objectSize, static_cast<unsigned char>(c));
				got++;
			}
			EXPECT_MSG(got == 64, "size class " + std::to_string(c));
			for (int i = 0; i < got; ++i) cache->Deallocate(objects[i], c);
		}
		CHECK_HEAP();
	}

	TEST("every thread gets its own cache");
	{
		constexpr int Threads = 4;
		ThreadCache* seen[Threads] = {};
		std::atomic<int> arrived{0};
		std::thread threads[Threads];
		for (int i = 0; i < Threads; ++i)
		{
			threads[i] = std::thread([&seen, &arrived, i]
			{
				ThreadCache* mine = ThreadCache::GetCache();
				void* objects[128];
				for (void*& object : objects) object = mine->Allocate(Small);
				// Park until every thread holds its cache, so all of them are alive at
				// the same time (a recycled cache object would otherwise be reused by a
				// later thread).
				arrived.fetch_add(1);
				while (arrived.load() < Threads) std::this_thread::yield();
				seen[i] = mine;
				for (void* object : objects) mine->Deallocate(object, Small);
			});
		}
		for (std::thread& thread : threads) thread.join();

		for (int i = 0; i < Threads; ++i)
		{
			EXPECT(seen[i] != nullptr);
			EXPECT(seen[i] != cache);
			for (int j = 0; j < i; ++j) EXPECT(seen[i] != seen[j]);
		}
		// The worker threads exited, so only our own cache is left registered.
		EXPECT_EQ(ThreadCache::GetStats().CacheCount, 1u);
		CHECK_HEAP();
	}

	TEST("a thread cache is recycled when its thread exits");
	{
		std::atomic<ThreadCache*> other{nullptr};
		std::thread worker([&other]
		{
			ThreadCache* mine = ThreadCache::GetCache();
			void* object = mine->Allocate(Small);
			mine->Deallocate(object, Small);
			other.store(mine);
		});
		worker.join();
		EXPECT(other.load() != nullptr);
		EXPECT_EQ(ThreadCache::GetStats().CacheCount, 1u);
		CHECK_HEAP();
	}

	TEST("BecomeIdle drops this thread's cache");
	{
		ThreadCache::BecomeIdle();
		EXPECT(ThreadCache::GetCacheIfPresent() == nullptr);
		EXPECT_EQ(ThreadCache::GetStats().CacheCount, 0u);
		ThreadCache* fresh = ThreadCache::GetCache();
		EXPECT(fresh != nullptr);
		void* object = fresh->Allocate(Small);
		EXPECT(object != nullptr);
		fresh->Deallocate(object, Small);
		CHECK_HEAP();
	}

	TEST("dropping a cache returns its objects to the central cache");
	{
		ThreadCache* mine = ThreadCache::GetCache();
		void* objects[256];
		for (void*& object : objects) object = mine->Allocate(Small);
		for (void* object : objects) mine->Deallocate(object, Small);

		PenMemoryStats beforeDrop{};
		GetStats(&beforeDrop);
		ThreadCache::BecomeIdle();
		PenMemoryStats afterDrop{};
		GetStats(&afterDrop);
		// Nothing is attributed to a thread cache any more, and everything it held
		// moved into the central cache or back into the page cache.
		EXPECT_EQ(afterDrop.ThreadCacheBytes, 0u);
		EXPECT_EQ(afterDrop.ThreadCacheCount, 0u);
		EXPECT_GE(afterDrop.CentralCacheBytes + afterDrop.PageFreeBytes,
		  beforeDrop.CentralCacheBytes + beforeDrop.PageFreeBytes +
			beforeDrop.ThreadCacheBytes);
		CHECK_HEAP();
	}

	TEST("the global budget can be resized");
	{
		ThreadCache::SetOverallThreadCacheSize(2 * MaxThreadCacheSize);
		EXPECT_EQ(ThreadCache::OverallThreadCacheSize(), 2 * MaxThreadCacheSize);
		ThreadCache::SetOverallThreadCacheSize(1);  // clipped to the minimum
		EXPECT_EQ(ThreadCache::OverallThreadCacheSize(),
		  static_cast<Usize>(MinThreadCacheSize));
		ThreadCache::SetOverallThreadCacheSize(DefaultOverallThreadCacheSize);
		EXPECT_EQ(ThreadCache::OverallThreadCacheSize(),
		  static_cast<Usize>(DefaultOverallThreadCacheSize));
		ThreadCache* mine = ThreadCache::GetCache();
		EXPECT(mine != nullptr);
		CHECK_HEAP();
	}

	return PenMemoryTest::Summary("thread cache");
}
