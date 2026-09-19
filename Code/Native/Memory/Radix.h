// File /Native/Memory/Radix.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: page map (radix tree).
//
// Maps a page id to the Span that owns that page.  This is what makes
// size-less deallocation possible: Deallocate(ptr) computes the page containing
// ptr, looks the Span up here and reads the size class (and the span state)
// out of it.
//
// Equivalent to tcmalloc's pagemap.h / PageMap3, with two simplifications:
//   * only a Span* is stored (tcmalloc redundantly packs the size class into
//     the same leaf slot to avoid touching the Span cache line);
//   * every page of a span is registered, not just its first page, so an
//     interior pointer inside a large span resolves to its owning span and can
//     be reported instead of silently corrupting the heap.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Common.hpp"
#include "OSMemory.h"

namespace PenMemory
{
	class Span;  // The page map only ever stores Span pointers.

	// Three-level radix tree mapping a BITS-wide key to a T*.
	//
	// Keys are page ids, so BITS = 48 - PageShift = 35 for the default
	// configuration: 12 bits of leaf, 12 bits of middle node, 11 bits of root.
	// A leaf covers 2^12 pages = 32 MiB of address space, which is also what
	// tcmalloc's PageMap3 ends up with.
	template <typename T, int BITS>
	class RadixTree
	{
	public:
		static constexpr int LeafBits = (BITS + 2) / 3;  // round up
		static constexpr int MidBits = (BITS + 2) / 3;   // round up
		static constexpr int RootBits = BITS - LeafBits - MidBits;
		static_assert(RootBits > 0, "too many bits assigned to leaf and mid");
		static_assert(RootBits < 31, "root array would be unreasonably large");

		static constexpr Usize LeafLength = Usize{1} << LeafBits;
		static constexpr Usize MidLength = Usize{1} << MidBits;
		static constexpr Usize RootLength = Usize{1} << RootBits;
		// Bytes of address space covered by one leaf.
		static constexpr Usize LeafCoveredBytes = LeafLength << PageShift;

		constexpr RadixTree() : m_root{} {}
		RadixTree(const RadixTree&) = delete;
		RadixTree& operator=(const RadixTree&) = delete;
		// Nodes are deliberately never freed: the page map is process-wide metadata.
		~RadixTree() = default;

		// Returns the value stored at `k`, or nullptr when the key was never set
		// (or the covering leaf was never created).
		PEN_MEMORY_ALWAYS_INLINE T* Get(Usize k) const
		{
			if (PEN_MEMORY_PREDICT_FALSE((k >> BITS) != 0)) return nullptr;
			const Node* node = m_root[k >> (LeafBits + MidBits)];
			if (PEN_MEMORY_PREDICT_FALSE(node == nullptr)) return nullptr;
			const Leaf* leaf = node->leafs[(k >> LeafBits) & (MidLength - 1)];
			if (PEN_MEMORY_PREDICT_FALSE(leaf == nullptr)) return nullptr;
			return leaf->values[k & (LeafLength - 1)];
		}

		// Stores `v` at `k`.  REQUIRES: Ensure() covered `k`.
		void Set(Usize k, T* v)
		{
			PEN_MEMORY_ASSERT((k >> BITS) == 0);
			Node* node = m_root[k >> (LeafBits + MidBits)];
			PEN_MEMORY_ASSERT(node != nullptr);
			Leaf* leaf = node->leafs[(k >> LeafBits) & (MidLength - 1)];
			PEN_MEMORY_ASSERT(leaf != nullptr);
			leaf->values[k & (LeafLength - 1)] = v;
		}

		// Makes [start, start + n) addressable.  Returns false if the range does not
		// fit in BITS or metadata allocation failed.
		bool Ensure(Usize start, Usize n)
		{
			if (n == 0) return true;
			if (((start + n - 1) >> BITS) != 0) return false;
			Usize key = start;
			while (key <= start + n - 1)
			{
				const Usize i1 = key >> (LeafBits + MidBits);
				const Usize i2 = (key >> LeafBits) & (MidLength - 1);
				Node* node = m_root[i1];
				if (node == nullptr)
				{
					node = static_cast<Node*>(os::MetaDataAlloc(sizeof(Node)));
					if (node == nullptr) return false;
					std::memset(node, 0, sizeof(Node));
					m_bytesUsed += sizeof(Node);
					m_root[i1] = node;
				}
				if (node->leafs[i2] == nullptr)
				{
					Leaf* leaf = static_cast<Leaf*>(os::MetaDataAlloc(sizeof(Leaf)));
					if (leaf == nullptr) return false;
					std::memset(leaf, 0, sizeof(Leaf));
					m_bytesUsed += sizeof(Leaf);
					node->leafs[i2] = leaf;
				}
				// Jump to the first key covered by the next leaf.
				key = ((key >> LeafBits) + 1) << LeafBits;
			}
			return true;
		}

		Usize BytesUsed() const { return m_bytesUsed + sizeof(*this); }
		static constexpr Usize RootSize() { return sizeof(m_root); }

	private:
		struct Leaf
		{
			T* values[LeafLength];
		};
		struct Node
		{
			Leaf* leafs[MidLength];
		};

		Node* m_root[RootLength];
		Usize m_bytesUsed = 0;
	};

	// The page map used by the allocator: page id -> owning Span.
	class PageMap
	{
	public:
		PageMap() = default;
		PageMap(const PageMap&) = delete;
		PageMap& operator=(const PageMap&) = delete;

		// Returns the Span owning page `p`, or nullptr if the page was never handed
		// out by the page cache.  Lock free: entries are written while the page heap
		// lock is held, but readers (the deallocation path) only need the mapping to
		// be stable for the span they are freeing.
		//
		// Kept inline because it is on the deallocation hot path.
		PEN_MEMORY_ALWAYS_INLINE Span* Get(PageId p) const { return m_tree.Get(p.Index()); }

		// Stores `span` for page `p`.  Defined in Radix.cpp.
		void Set(PageId p, Span* span);

		// Makes [r.Page, r.Page + r.NumPages) addressable, allocating radix nodes as
		// needed.  Defined in Radix.cpp.
		bool Ensure(Range r);

		Usize BytesUsed() const;

		static constexpr Usize RootSize()
		{
			return RadixTree<Span, PageIdBits>::RootSize();
		}
		static constexpr Usize LeafCoveredBytes()
		{
			return RadixTree<Span, PageIdBits>::LeafCoveredBytes;
		}

	private:
		RadixTree<Span, PageIdBits> m_tree;
	};

}  // namespace PenMemory
