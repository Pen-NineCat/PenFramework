// File /Native/Engine/Core/DeferredDestroyQueue.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "DeferredDestroyQueue.h"

#include <vector>

namespace PenEngine
{
	void DeferredDestroyQueue::PostDeferredObject(PObject* object)
	{
		m_deferredDestroyObjects.emplace(object,0);
	}

	void DeferredDestroyQueue::CatchDeferredObject(PObject* object)
	{
		// 在这里做防御是多余的，因为PObject的DestroyLater逻辑与用户无关
		m_deferredDestroyObjects[object]++;
	}

	void DeferredDestroyQueue::ReleaseDeferredObject(PObject* object)
	{
		// 在这里做防御是多余的，因为PObject的DestroyLater逻辑与用户无关
		m_deferredDestroyObjects[object]--;
	}

	void DeferredDestroyQueue::Update()
	{
		// 分两步：先摘出计数归零的对象并擦表，再逐个删除。
		// delete 会执行 ~PObject -> DestroySignal -> 各系统的回调，那些回调可能继续增删本表
		// （ReleaseDeferredObject/CatchDeferredObject 都走 operator[]，插入会 rehash），
		// 因此删除过程中绝不能有任何指向本表的迭代器或引用存活
		std::vector<PObject*> doomedObjects;

		for (auto it = m_deferredDestroyObjects.begin(); it != m_deferredDestroyObjects.end(); )
		{
			if (it->second != 0)
			{
				++it;
				continue;
			}

			doomedObjects.push_back(it->first);
			it = m_deferredDestroyObjects.erase(it); // erase 返回下一个有效迭代器，不能再 ++
		}

		for (PObject* object : doomedObjects)
			delete object;
	}
}
