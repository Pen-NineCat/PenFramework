// File /Native/Memory/Test/Benchmark/benchmark.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Throughput sanity check: PenMemory against the platform allocator.
//
// This is not part of the test suite (build.ps1 builds it but only runs it with
// -Bench).  It exists to show that the three layer design actually keeps the
// allocation path short: a thread-local pop for the hot case, a batched central
// cache hop for a miss, and the page cache only when a span runs dry.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "Interface.h"

#include "../../Common.hpp"

using namespace PenMemory;

namespace
{

	constexpr Usize Iterations = 4000000;
	constexpr Usize LiveObjects = 64;  // working set per thread

	double NowSeconds()
	{
		using clock = std::chrono::steady_clock;
		return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
	}

	// Allocation/free pairs of `size`, keeping a small live set so the caches stay
	// warm, which is what a real workload looks like.
	template <typename AllocFn, typename FreeFn>
	double RunWorkload(Usize size, Usize iterations, AllocFn alloc, FreeFn free)
	{
		std::vector<void*> live(LiveObjects, nullptr);
		// Warm up so that the first-touch costs (span growth, page faults) do not
		// dominate the measurement.
		for (Usize warmup = 0; warmup < 100000; ++warmup)
		{
			void* p = alloc(size);
			free(p);
		}
		const double start = NowSeconds();
		for (Usize i = 0; i < iterations; ++i)
		{
			const Usize slot = i % LiveObjects;
			if (live[slot] != nullptr) free(live[slot]);
			void* p = alloc(size);
			*static_cast<volatile char*>(p) = 1;
			live[slot] = p;
		}
		const double elapsed = NowSeconds() - start;
		for (void* p : live)
		{
			if (p != nullptr) free(p);
		}
		return elapsed;
	}

	void Report(const char* label, Usize size, double seconds)
	{
		const double mops = static_cast<double>(Iterations) / seconds / 1e6;
		std::printf("  %-28s %6zu B : %7.2f s  %8.2f Mops/s\n", label, size, seconds,
					mops);
	}

}  // namespace

int main()
{
	std::printf("PenMemory benchmark (%zu alloc/free pairs per case)\n",
				Iterations);

	const Usize sizes[] = {8, 32, 128, 1024, 8192, 131072};
	for (Usize size : sizes)
	{
		const double ours = RunWorkload(size, Iterations,
										[](Usize n) { return Allocate(n); },
										[](void* p) { Deallocate(p); });
		const double theirs = RunWorkload(
			size, Iterations, [](Usize n) { return std::malloc(n); },
			[](void* p) { std::free(p); });
		Report("PenMemory", size, ours);
		Report("platform malloc", size, theirs);
		std::printf("      -> speedup %.2fx\n", theirs / ours);
	}

	// Multi-threaded: the interesting case for a per-thread cache.
	constexpr int Threads = 4;
	for (Usize size : {Usize{32}, Usize{1024}, Usize{8192}})
	{
		auto runThreads = [](Usize sz, bool ours)
		{
			std::vector<std::thread> threads;
			const double start = NowSeconds();
			for (int t = 0; t < Threads; ++t)
			{
				threads.emplace_back([sz, ours]
				{
					if (ours)
					{
						RunWorkload(sz, Iterations, [](Usize n) { return Allocate(n); },
									[](void* p) { Deallocate(p); });
					}
					else
					{
						RunWorkload(
							sz, Iterations, [](Usize n) { return std::malloc(n); },
							[](void* p) { std::free(p); });
					}
				});
			}
			for (std::thread& thread : threads) thread.join();
			return NowSeconds() - start;
		};
		const double ours = runThreads(size, true);
		const double theirs = runThreads(size, false);
		std::printf("  %d threads %-18s %6zu B : ours %6.2f s (%6.2f Mops/s), "
					"platform %6.2f s (%6.2f Mops/s)\n",
					Threads, "PenMemory", size, ours,
					static_cast<double>(Threads * Iterations) / ours / 1e6,
					theirs,
					static_cast<double>(Threads * Iterations) / theirs / 1e6);
	}

	PenMemoryStats stats{};
	GetStats(&stats);
	std::printf(
		"\n  heap: system %.1f MiB, page-free %.1f MiB, released %.1f MiB, "
		"central %.1f MiB, thread %.2f MiB, metadata %.0f KiB\n",
		stats.SystemBytes / 1048576.0, stats.PageFreeBytes / 1048576.0,
		stats.PageReleasedBytes / 1048576.0,
		stats.CentralCacheBytes / 1048576.0,
		stats.ThreadCacheBytes / 1048576.0, stats.MetadataBytes / 1024.0);
	return 0;
}
