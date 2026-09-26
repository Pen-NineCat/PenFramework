// File /Native/Engine/Core/DeferredDestroyQueue.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Object/PObject.h"
#include "../Utils/OwnerThread.hpp"
#include "../Utils/Singleton.hpp"
#include <boost/unordered/unordered_flat_map.hpp>

namespace PenEngine
{
	/// @brief 全局延迟销毁队列（引用计数协议）
	/// 对象触发 DestroyLater 后向队列注册自身，并可能被多个系统监听到：每个系统通过Push占用一个计数并维护自己的完成检查，
	/// 完成时调用Release减一；本队列在每帧Update中delete所有计数归零的对象。
	class DeferredDestroyQueue : public Singleton<DeferredDestroyQueue>, SignalObject
	{
	public:
		DeferredDestroyQueue() noexcept { s_alive = true; }
		virtual ~DeferredDestroyQueue() noexcept override { s_alive = false; }

		void PostDeferredObject(PObject* object);
		void CatchDeferredObject(PObject* object);
		void ReleaseDeferredObject(PObject* object);

		void Update();

		/// @brief 队列实例是否仍然存活
		/// @note 函数局部静态量的析构顺序不可控，队列可能先于使用者析构；
		///       此时任何 GetInstance() 都是对已析构对象的访问，使用者应先询问本函数
		[[nodiscard]] static bool IsAlive() noexcept { return s_alive; }

		/// @brief 对象是否已经进入延迟销毁队列（即已经调用过 DestroyLater）
		[[nodiscard]] bool IsDeferred(PObject* object) const noexcept
		{
			return m_deferredDestroyObjects.find(object) != m_deferredDestroyObjects.end();
		}

		/// @brief 判断当前线程是否为队列所属线程（单例在主线程首次构造 ⇒ 即主线程）
		[[nodiscard]] bool IsOwnerThread() const noexcept
		{
			return m_ownerThread.IsOwner();
		}
	private:
		/// @note 常量初始化（不参与动态析构），因此可在任何时刻安全查询
		inline static bool s_alive = false;

		/// @brief 归属线程：本队列的增删与遍历都只允许在它上面发生。
		///        四个入口（Post/Catch/Release/Update）都做校验：Debug 报告并中断，
		///        Release 抛 `CorePluginThreadViolation`（见 .cpp）
		OwnerThread m_ownerThread;

		boost::unordered::unordered_flat_map<PObject*, Usize> m_deferredDestroyObjects;
	};
}
