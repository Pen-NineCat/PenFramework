// File /Native/Memory/tests/test_override.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Runs the whole test with PenMemory as the global operator new/delete.
// The standard library containers below therefore allocate through the
// allocator under test, including their growth, alignment and destruction
// paths.

#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "MiniTest.h"

using namespace PenMemory;

namespace
{

	struct alignas(128) OverAligned
	{
		unsigned char payload[128];
	};

	struct Node
	{
		int key;
		std::string value;
	};

}  // namespace

int main()
{
	TEST("std::vector growth and destruction");
	{
		std::vector<std::string> values;
		for (int i = 0; i < 20000; ++i)
		{
			values.push_back(std::string(static_cast<Usize>(i % 97) + 1, 'x') +
			   std::to_string(i));
		}
		EXPECT_EQ(values.size(), 20000u);
		bool ok = true;
		for (int i = 0; i < 20000; ++i)
		{
			if (values[i] != std::string(static_cast<Usize>(i % 97) + 1, 'x') +
				  std::to_string(i))
			{
				ok = false;
			}
		}
		EXPECT(ok);
	}
	CHECK_HEAP();

	TEST("std::map and std::shared_ptr");
	{
		std::map<int, std::string> table;
		for (int i = 0; i < 20000; ++i)
		{
			table[i] = std::string(static_cast<Usize>(i % 53) + 1, 'y');
		}
		EXPECT_EQ(table.size(), 20000u);
		EXPECT_EQ(table[19999].size(), static_cast<Usize>(19999 % 53) + 1);

		std::vector<std::shared_ptr<std::string>> shared;
		for (int i = 0; i < 5000; ++i)
		{
			shared.push_back(std::make_shared<std::string>(static_cast<Usize>(i % 11) + 1, 'z'));
		}
		for (const auto& ptr : shared) EXPECT(ptr != nullptr);
		EXPECT_EQ(shared[4999]->size(), static_cast<Usize>(4999 % 11) + 1);
	}
	CHECK_HEAP();

	TEST("over-aligned allocations");
	{
		std::vector<std::unique_ptr<OverAligned>> objects;
		for (int i = 0; i < 1000; ++i)
		{
			auto object = std::make_unique<OverAligned>();
			EXPECT(IsAlignedTo(object.get(), alignof(OverAligned)));
			object->payload[i % 128] = static_cast<unsigned char>(i);
			objects.push_back(std::move(object));
		}
		for (int i = 0; i < 1000; ++i)
		{
			EXPECT_EQ(objects[static_cast<Usize>(i)]->payload[i % 128],
			static_cast<unsigned char>(i));
		}
	}
	CHECK_HEAP();

	TEST("stl containers across threads");
	{
		std::thread workers[4];
		for (int t = 0; t < 4; ++t)
		{
			workers[t] = std::thread([t]
			{
				std::vector<Node> nodes;
				for (int i = 0; i < 4000; ++i)
				{
					nodes.push_back(Node{i, std::string(static_cast<Usize>(i % 31) + 1, 'w')});
				}
				std::map<int, Node> index;
				for (const Node& node : nodes) index[node.key] = node;
				for (int i = 0; i < 4000; ++i)
				{
					if (index[i].value.size() != static_cast<Usize>(i % 31) + 1)
					{
						PenMemoryTest::Fail(__FILE__, __LINE__, "container content mismatch");
						break;
					}
				}
				(void)t;
			});
		}
		for (std::thread& worker : workers) worker.join();
	}
	CHECK_HEAP();

	TEST("new / delete round trips through the allocator");
	{
		PenMemoryStats before{};
		GetStats(&before);
		void* ptr = ::operator new(1234);
		EXPECT(ptr != nullptr);
		EXPECT_GE(GetAllocatedSize(ptr), 1234u);
		std::memset(ptr, 0x5a, 1234);
		::operator delete(ptr);

		void* array = ::operator new[](50000);
		EXPECT(array != nullptr);
		::operator delete[](array);

		void* aligned = ::operator new(4096, std::align_val_t{512});
		EXPECT(IsAlignedTo(aligned, 512));
		::operator delete(aligned, std::align_val_t{512});

		void* nothrowPtr = ::operator new(999, std::nothrow);
		EXPECT(nothrowPtr != nullptr);
		::operator delete(nothrowPtr, std::nothrow);
		(void)before;
	}
	CHECK_HEAP();

	return PenMemoryTest::Summary("global new/delete");
}
