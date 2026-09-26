// File /Native/Engine/Thread/Internal/ThreadPlatform.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "ThreadPlatform.h"

#include <algorithm>
#include <atomic>
#include <boost/dynamic_bitset.hpp>
#include <thread>
#include <vector>

#ifdef PENFRAMEWORK_OS_WIN32
#include "../../OS/Windows/Windows.h"
#else
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

namespace PenEngine::Internal
{
	namespace
	{
		/// @brief 处理器拓扑缓存
		/// @note 两个位集在首次查询后只读；两个游标是原子的，用于把连续的 worker 摊到不同核上
		struct CoreTopology
		{
			/// 全部逻辑处理器（位下标 = CoreID）
			boost::dynamic_bitset<> AllLogical;

			/// 高性能核（efficiencyClass 最大者）；机器只有一类核时 = AllLogical
			boost::dynamic_bitset<> HighEfficiency;

			/// 能效核（efficiencyClass 最小者）；机器只有一类核时**为空**（表示不存在更低能效的核）
			boost::dynamic_bitset<> LowEfficiency;

			std::atomic<Usize> HighCursor{ 0 };
			std::atomic<Usize> LowCursor{ 0 };
		};

#ifdef PENFRAMEWORK_OS_WIN32
		/// @brief Windows 处理器组内的位数（x64 为 64，Win32 为 32）
		constexpr Usize ProcessorGroupBits = sizeof(KAFFINITY) * 8;

		/// @brief 遍历 RelationProcessorCore 的每个物理核条目
		/// @tparam Visitor `void(BYTE efficiencyClass, const GROUP_AFFINITY& Mask)`
		/// @return 枚举是否成功（失败的两种情形：接口报错、缓冲区长度为 0）
		/// @note 用两趟调用（先问长度、再取数据）是这套 API 的固定用法
		template <typename Visitor>
		[[nodiscard]] bool ForEachProcessorCore(Visitor&& visit) noexcept
		{
			DWORD bufferSize = 0;

			::SetLastError(ERROR_SUCCESS);

			if (::GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bufferSize) != 0
				|| ::GetLastError() != ERROR_INSUFFICIENT_BUFFER
				|| bufferSize == 0)
			{
				return false;
			}

			std::vector<U8> buffer(bufferSize);
			auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());

			if (::GetLogicalProcessorInformationEx(RelationProcessorCore, info, &bufferSize) == 0)
				return false;

			DWORD consumed = 0;

			while (consumed < bufferSize)
			{
				auto* entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + consumed);

				// 防御：正常情况下 size 恒 > 0，为 0 说明结构被写坏，宁可提前收手也不要死循环
				if (entry->Size == 0)
					break;

				consumed += entry->Size;

				if (entry->Relationship != RelationProcessorCore)
					continue;

				const BYTE efficiencyClass = entry->Processor.EfficiencyClass;
				const WORD groupCount = entry->Processor.GroupCount;

				for (WORD group = 0; group < groupCount; ++group)
					visit(efficiencyClass, entry->Processor.GroupMask[group]);
			}

			return true;
		}
#endif // PENFRAMEWORK_OS_WIN32

		/// @brief 查询处理器拓扑，直接写进调用方提供的对象
		/// @note 刻意用输出参数而不是返回值：CoreTopology 含原子成员（不可拷贝、不可移动），
		///       而"具名局部量的返回值"不享受保证拷贝消除（NRVO 不是强制的）
		void QueryTopology(CoreTopology& out) noexcept
		{
			const auto fallback = [&out]() noexcept
			{
				// 查询不可用时的兜底：按 STL 报告的数量铺满，并全部视为高性能核
				// （这样 OnHighEfficiencyCore 仍可用，OnLowEfficiencyCore 明确失败）
				const U32 count = std::max<U32>(1, std::thread::hardware_concurrency());
				out.AllLogical.resize(count);
				out.AllLogical.set();
				out.HighEfficiency = out.AllLogical;
				out.LowEfficiency.clear();
			};

#ifdef PENFRAMEWORK_OS_WIN32
			BYTE minClass = 0xFF;
			BYTE maxClass = 0;
			Usize maxIndex = 0;

			const bool enumerated = ForEachProcessorCore(
				[&minClass, &maxClass, &maxIndex](BYTE efficiencyClass, const GROUP_AFFINITY& Mask) noexcept
				{
					minClass = std::min(minClass, efficiencyClass);
					maxClass = std::max(maxClass, efficiencyClass);

					for (Usize bit = 0; bit < ProcessorGroupBits; ++bit)
					{
						if (((Mask.Mask >> bit) & 1ull) == 0)
							continue;

						maxIndex = std::max(maxIndex, static_cast<Usize>(Mask.Group) * 64 + bit);
					}
				});

			if (!enumerated)
			{
				fallback();
				return;
			}

			out.AllLogical.resize(maxIndex + 1);
			out.HighEfficiency.resize(maxIndex + 1);
			out.LowEfficiency.resize(maxIndex + 1);

			// 只有真正存在两类核时才认为"有能效核"：单一类别 ⇒ LowEfficiency 保持为空
			const bool hybrid = minClass != maxClass;

			const bool filled = ForEachProcessorCore([&out, minClass, maxClass, hybrid](BYTE efficiencyClass, const GROUP_AFFINITY& Mask) noexcept
			{
				for (Usize bit = 0; bit < ProcessorGroupBits; ++bit)
				{
					if (((Mask.Mask >> bit) & 1ull) == 0)
						continue;

					const Usize index = static_cast<Usize>(Mask.Group) * 64 + bit;

					out.AllLogical.set(index);

					if (efficiencyClass == maxClass)
						out.HighEfficiency.set(index);

					if (hybrid && efficiencyClass == minClass)
						out.LowEfficiency.set(index);
				}
			});

			// 第二趟失败：这一次可能已经把部分位集写脏，整体退回兜底结果
			if (!filled)
			{
				fallback();
				return;
			}
#else
			// POSIX：Windows 之外的平台未在本机验证（见模块说明）
			// 能效核在 POSIX 侧没有等价查询，故 LowEfficiency 保持为空 ⇒ OnLowEfficiencyCore 明确失败
			const long online = ::sysconf(_SC_NPROCESSORS_ONLN);
			const U32 count = online > 0
				? static_cast<U32>(online)
				: std::max<U32>(1, std::thread::hardware_concurrency());

			out.AllLogical.resize(count);
			out.AllLogical.set();
			out.HighEfficiency = out.AllLogical;
#endif // PENFRAMEWORK_OS_WIN32

			if (out.AllLogical.none())
				fallback();
		}

		/// @brief 拓扑缓存的持有者
		/// @note 用带构造函数的持有者而不是直接 `static CoreTopology cached = QueryTopology();`：
		///       后者要求可拷贝/可移动，而 CoreTopology 含原子成员。函数局部 static 的初始化
		///       由 C++ 保证线程安全，构造完成后拓扑只读（两个游标是原子）
		struct CoreTopologyHolder
		{
			CoreTopology Value;

			CoreTopologyHolder() noexcept
			{
				QueryTopology(Value);
			}
		};

		[[nodiscard]] CoreTopology& Topology() noexcept
		{
			static CoreTopologyHolder holder;
			return holder.Value;
		}

		/// @brief 从位集里按轮转取下一个核心下标
		/// @param cursor 轮转游标（原子，多个线程同时取核也不会取到同一个下标）
		[[nodiscard]] bool SelectCore(const boost::dynamic_bitset<>& cores, std::atomic<Usize>& cursor, CoreID& outIndex) noexcept
		{
			if (cores.none())
				return false;

			const Usize size = cores.size();
			const Usize start = cursor.load(std::memory_order_relaxed) % size;

			Usize found = cores.find_next(start);

			// 回绕：本轮之后没有更多核心时从头再找，避免"游标走到末尾就把策略判成失败"
			if (found == boost::dynamic_bitset<>::npos)
				found = cores.find_first();

			if (found == boost::dynamic_bitset<>::npos)
				return false;

			cursor.store((found + 1) % size, std::memory_order_relaxed);
			outIndex = static_cast<CoreID>(found);
			return true;
		}
	}

	NativeThreadID ThisThreadID() noexcept
	{
#ifdef PENFRAMEWORK_OS_WIN32
		return static_cast<NativeThreadID>(::GetCurrentThreadId());
#else
		return static_cast<NativeThreadID>(::pthread_self());
#endif
	}

	NativeThreadHandle ThisThreadNativeHandle() noexcept
	{
#ifdef PENFRAMEWORK_OS_WIN32
		// 伪句柄：仅对"本线程"有效，不需要也**不可以** CloseHandle
		return static_cast<NativeThreadHandle>(::GetCurrentThread());
#else
		return static_cast<NativeThreadHandle>(::pthread_self());
#endif
	}

	CoreID ThisCoreIndex() noexcept
	{
#ifdef PENFRAMEWORK_OS_WIN32
		PROCESSOR_NUMBER Number{};
		::GetCurrentProcessorNumberEx(&Number);

		return static_cast<CoreID>(Number.Group) * 64u + Number.Number;
#else
		const int Cpu = ::sched_getcpu();

		return Cpu < 0 ? 0u : static_cast<CoreID>(Cpu);
#endif
	}

	U32 HardwareConcurrency() noexcept
	{
		const Usize count = Topology().AllLogical.count();

		if (count == 0)
			return std::max<U32>(1, std::thread::hardware_concurrency());

		return static_cast<U32>(count);
	}

	NativeThreadID ThreadIDOf(NativeThreadHandle handle) noexcept
	{
		if (handle == NativeThreadHandle{})
			return 0;

#ifdef PENFRAMEWORK_OS_WIN32
		// 注意：伪句柄（GetCurrentThread 的返回值）在此处取不到 ID，会返回 0
		return static_cast<NativeThreadID>(::GetThreadId(static_cast<HANDLE>(handle)));
#else
		return static_cast<NativeThreadID>(handle);
#endif
	}

	bool LockThreadCoreToIndex(NativeThreadHandle handle, CoreID index) noexcept
	{
#ifdef PENFRAMEWORK_OS_WIN32
		GROUP_AFFINITY affinity = {};
		affinity.Group = static_cast<WORD>(index / 64u);
		affinity.Mask = static_cast<KAFFINITY>(1) << (index % ProcessorGroupBits);

		const HANDLE target = handle == NativeThreadHandle{} ? ::GetCurrentThread() : static_cast<HANDLE>(handle);

		// 用 SetThreadGroupAffinity 而不是 SetThreadAffinityMask：
		// 后者的掩码只对调用线程所在的处理器组生效，跨组时会静默绑错
		return ::SetThreadGroupAffinity(target, &affinity, nullptr) != 0;
#else
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(static_cast<int>(index), &set);

		const pthread_t target = handle == NativeThreadHandle{} ? ::pthread_self() : static_cast<pthread_t>(handle);

		return ::pthread_setaffinity_np(target, sizeof(set), &set) == 0;
#endif
	}

	bool UnlockThreadCore(NativeThreadHandle handle) noexcept
	{
#ifdef PENFRAMEWORK_OS_WIN32
		DWORD_PTR processMask = 0;
		DWORD_PTR systemMask = 0;

		if (::GetProcessAffinityMask(::GetCurrentProcess(), &processMask, &systemMask) == 0)
			return false;

		const HANDLE target = handle == NativeThreadHandle{} ? ::GetCurrentThread() : static_cast<HANDLE>(handle);

		return ::SetThreadAffinityMask(target, processMask) != 0;
#else
		cpu_set_t set;
		CPU_ZERO(&set);

		const long online = ::sysconf(_SC_NPROCESSORS_ONLN);

		for (long index = 0; index < online && index < CPU_SETSIZE; ++index)
			CPU_SET(static_cast<int>(index), &set);

		const pthread_t target = handle == NativeThreadHandle{} ? ::pthread_self() : static_cast<pthread_t>(handle);

		return ::pthread_setaffinity_np(target, sizeof(set), &set) == 0;
#endif
	}

	bool LockThreadCore(NativeThreadHandle handle, LockThreadCorePolicy policy) noexcept
	{
		CoreTopology& cached = Topology();
		CoreID index = 0;

		switch (policy)
		{
			case LockThreadCorePolicy::OnCurrentCore:
				return LockThreadCoreToIndex(handle, ThisCoreIndex());

			case LockThreadCorePolicy::OnHighEfficiencyCore:
				if (!SelectCore(cached.HighEfficiency, cached.HighCursor, index))
					return false;

				return LockThreadCoreToIndex(handle, index);

			case LockThreadCorePolicy::OnLowEfficiencyCore:
				// 机器只有一类核时 LowEfficiency 为空：不存在"更低能效的核"，明确失败
				if (!SelectCore(cached.LowEfficiency, cached.LowCursor, index))
					return false;

				return LockThreadCoreToIndex(handle, index);

			case LockThreadCorePolicy::OnAnyCore:
				return UnlockThreadCore(handle);
		}

		return false;
	}
}
