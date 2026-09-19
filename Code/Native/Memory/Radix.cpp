// File /Native/Memory/Radix.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// PenMemory: page map (radix tree), out-of-line parts.
//
// The tree itself is a template and therefore lives in Radix.h; only the
// PageMap wrapper's cold paths are compiled here.  PageMap::Get stays inline
// because it runs on every deallocation.

#include "Radix.h"

namespace PenMemory
{
	void PageMap::Set(PageId p, Span* span) { m_tree.Set(p.Index(), span); }

	bool PageMap::Ensure(Range r)
	{
		// The page cache calls this once per region obtained from the OS.
		return m_tree.Ensure(r.Page.Index(), r.NumPages.RawNum());
	}

	Usize PageMap::BytesUsed() const { return m_tree.BytesUsed(); }

}  // namespace PenMemory
