// File /Native/Engine/Object/RefObject.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "PObject.h"

namespace PenEngine
{
	/// @brief 引用计数对象
	/// @note RefObject 不是一个线程安全对象，请仔细考虑其在多线程环境下的使用 
	class RefObject : public PObject
	{
		DECL_P_OBJECT(RefObject, PObject);
	public:
		RefObject() noexcept = default;

		RefObject(const RefObject&) = delete;
		RefObject(RefObject&&) = default;
		RefObject& operator=(const RefObject&) = delete;
		RefObject& operator=(RefObject&&) = default;

		virtual ~RefObject() noexcept override = default;

		Usize Catch() noexcept
		{
			// 上溢保护
			DEBUG_VERIFY_REPORT(m_refCount < std::numeric_limits<Usize>::max(), "Exceed max times");

			++m_refCount;
			return m_refCount;
		}

		Usize Release() noexcept
		{
			// 下溢保护
			DEBUG_VERIFY_REPORT(m_refCount > 0, "Exceed max times");

			--m_refCount;

			Usize tmp = m_refCount;

			if (m_refCount == 0)
				delete this;

			return tmp;
		}

		[[nodiscard]] Usize GetRefCount() const noexcept
		{
			return m_refCount;
		}
	private:
		Usize m_refCount = 0;
	};

	namespace Internal
	{
		struct RefPtrData
		{
			struct WeakData
			{
				Usize Count = 0;
				bool Safe = false;
			};

			WeakData* WeakData = nullptr;
			RefObject* Object = nullptr;

			/// @brief 从裸指针接管对象，并显式建立控制块（与 shared_ptr 一致，控制块不采用惰性分配）
			/// @note 会分配控制块，因此不再保证 noexcept；先分配后取引用，分配失败时不会残留任何计数
			/// @note 传入 nullptr 时会清空持有的对象（等价于 reset）
			void ConstructFrom(RefObject* object)
			{
				if (object == nullptr)
				{
					Object = nullptr;
					return;
				}

				// 先分配控制块：若分配失败，此时尚未获取任何引用计数，也尚未改变持有的对象
				WeakData = new struct WeakData;

				Object = object;
				Object->Catch();

				WeakCatch();
				WeakData->Safe = true;
			}

			/// @brief 获取一次强引用，并同时持有控制块的一次弱引用计数
			void Catch() noexcept
			{
				if (Object)
					Object->Catch();

				WeakCatch();
			}

			/// @brief 释放一次强引用与对应的弱引用计数
			/// @note 释放后必须放弃对象指针，否则析构或 Reset 时会重复释放同一个对象
			void Release() noexcept
			{
				if (Object)
				{
					Usize count = Object->Release();
					Object = nullptr;

					if (count == 0 && WeakData)
						WeakData->Safe = false;
				}

				WeakRelease();
			}

			/// @brief 仅当自身是对象的唯一持有者（强引用计数为 1）时交出所有权，否则返回 nullptr
			/// @note 交出所有权即放弃控制块，全部监视该对象的弱引用指针随之失效
			RefObject* Detach() noexcept
			{
				if (Object == nullptr || Object->GetRefCount() > 1)
					return nullptr;

				if (WeakData)
					WeakData->Safe = false;

				WeakRelease();

				return std::exchange(Object, nullptr);
			}

			/// @brief 确保控制块存在并获取一次弱引用计数
			/// @note Safe 取决于当前是否仍持有对象，未持有对象的控制块一律视为失效
			void EnsureWeakCatch()
			{
				if (WeakData == nullptr)
				{
					if (Object == nullptr)
						return;

					WeakData = new struct WeakData;
				}

				WeakCatch();
				WeakData->Safe = (Object != nullptr);
			}

			void WeakCatch() noexcept
			{
				if (WeakData)
					++(WeakData->Count);
			}

			void WeakRelease() noexcept
			{
				if (WeakData)
				{
					DEBUG_VERIFY_REPORT(WeakData->Count > 0,"Invalid WeakCount");

					--(WeakData->Count);
					if (WeakData->Count == 0)
						delete WeakData;

					// 无论控制块是否被释放，持有者都必须放弃指针，避免同一份数据被重复释放
					WeakData = nullptr;
				}
			}
		};
	}
	template <typename T> requires std::derived_from<T, RefObject> || std::is_same_v<T, RefObject>
	class WeakRefPtr;
	template <typename T> requires std::derived_from<T, RefObject> || std::is_same_v<T, RefObject>
	class RefPtr;

	class BadRefPtr : public Exception
	{
	public:
		BadRefPtr() : Exception("BadRefPtr", "需要构造的目标RefObject对象不存在") {}
	};

	template <typename T> requires std::derived_from<T, RefObject> || std::is_same_v<T, RefObject>
	class RefPtr
	{
		template <typename U> requires std::derived_from<U, RefObject> || std::is_same_v<U, RefObject>
		friend class WeakRefPtr;
	public:
		constexpr RefPtr() noexcept = default;
		/* implicit */ constexpr RefPtr(std::nullptr_t) noexcept {}

		/// @note 接管对象时需要显式分配控制块，因此可能抛出（不再保证 noexcept）
		/* implicit */ RefPtr(T* object)
		{
			m_data.ConstructFrom(object);
		}

		explicit RefPtr(const WeakRefPtr<T>& weak)
		{
			if (weak.m_data.Object == nullptr || weak.m_data.WeakData == nullptr || weak.m_data.WeakData->Safe == false)
				ThrowException(BadRefPtr());

			m_data = weak.m_data;
			m_data.Catch();
		}

		RefPtr(const RefPtr& ref) noexcept
		{
			m_data = ref.m_data;
			m_data.Catch();
		}

		RefPtr(RefPtr&& ref) noexcept
		{
			std::swap(m_data, ref.m_data);
		}

		RefPtr& operator=(const RefPtr& ref) noexcept
		{
			if (this == &ref)
				return *this;

			m_data.Release();
			m_data = ref.m_data;
			m_data.Catch();

			return *this;
		}

		RefPtr& operator=(RefPtr&& ref) noexcept
		{
			if (this == &ref)
				return *this;

			m_data.Release();
			m_data = std::exchange(ref.m_data, {});

			return *this;
		}

		~RefPtr() noexcept
		{
			m_data.Release();
		}

		/// @note 重新接管对象时需要显式分配控制块，因此可能抛出（不再保证 noexcept）
		void Reset(T* oth = nullptr)
		{
			m_data.Release();
			m_data.ConstructFrom(oth);
		}

		/// @brief 将管理的资源从智能指针中分离
		/// @note 仅当自身是唯一持有者时可用；否则返回 nullptr
		/// @note 该操作会导致全部监视该智能指针所管理的资源的弱引用指针失效，因为无法判断指针是否仍然有效
		/// @note 分离后调用方接管该对象（对象引用计数仍为 1），需由调用方负责 Release 或 delete
		T* Detach() noexcept
		{
			return static_cast<T*>(m_data.Detach());
		}

		[[nodiscard]] Usize GetRefCount() const noexcept
		{
			return m_data.Object ? m_data.Object->GetRefCount() : 0;
		}

		[[nodiscard]] T* Get() const noexcept
		{
			return static_cast<T*>(m_data.Object);
		}

		T* operator->() const noexcept
		{
			return static_cast<T*>(m_data.Object);
		}

		T& operator*() const noexcept
		{
			return *static_cast<T*>(m_data.Object);
		}

		explicit operator bool() const noexcept
		{
			return m_data.Object != nullptr;
		}

		bool operator==(std::nullptr_t) const noexcept
		{
			return m_data.Object == nullptr;
		}

		bool operator==(const RefPtr& ref) const noexcept
		{
			return m_data.Object == ref.m_data.Object;
		}

		auto operator<=>(const RefPtr& ref) const noexcept
		{
			return m_data.Object <=> ref.m_data.Object;
		}

		auto operator<=>(std::nullopt_t) const noexcept
		{
			return m_data.Object <=> static_cast<RefObject*>(nullptr);
		}
	private:
		Internal::RefPtrData m_data;
	};

	template <typename T>  requires std::derived_from<T, RefObject> || std::is_same_v<T, RefObject>
	class WeakRefPtr
	{
		template <typename U> requires std::derived_from<U, RefObject> || std::is_same_v<U, RefObject>
		friend class RefPtr;
	public:
		constexpr WeakRefPtr() noexcept = default;
		constexpr explicit WeakRefPtr(std::nullptr_t) noexcept {}

		explicit WeakRefPtr(const RefPtr<T>& ref)
		{
			m_data = ref.m_data;
			m_data.EnsureWeakCatch();
		}

		WeakRefPtr(const WeakRefPtr& weak) noexcept
		{
			m_data = weak.m_data;
			m_data.WeakCatch();
		}

		WeakRefPtr& operator=(const WeakRefPtr& weak) noexcept
		{
			m_data.WeakRelease();
			m_data = weak.m_data;
			m_data.WeakCatch();
			return *this;
		}

		~WeakRefPtr() noexcept
		{
			m_data.WeakRelease();
		}

		WeakRefPtr(WeakRefPtr&& weak) noexcept
		{
			std::swap(m_data, weak.m_data);
		}

		WeakRefPtr& operator=(WeakRefPtr&& weak) noexcept
		{
			if (this == &weak)
				return *this;

			m_data.WeakRelease();
			m_data = {};
			std::swap(m_data, weak.m_data);

			return *this;
		}

		void Reset() noexcept
		{
			m_data.WeakRelease();
		}

		/// @brief 获得监视对象的强引用计数
		/// @note 对象已销毁或没有监视对象时返回 0（与 weak_ptr::use_count 一致），此时不会访问已销毁的对象
		[[nodiscard]] Usize GetRefCount() const noexcept
		{
			if (Expired())
				return 0;

			return m_data.Object->GetRefCount();
		}

		/// @brief 监视的对象是否已失效（与 weak_ptr::expired 语义一致）
		/// @note 没有监视对象、或对象已被销毁（强引用计数归零）时为 true
		[[nodiscard]] bool Expired() const noexcept
		{
			return m_data.Object == nullptr || m_data.WeakData == nullptr || m_data.WeakData->Safe == false;
		}

		/// @brief 监视的对象是否仍然存活且可以 Lock
		/// @note 等价于 !Expired()
		explicit operator bool() const noexcept
		{
			return !Expired();
		}

		RefPtr<T> Lock()
		{
			RefPtr<T> res;

			if (!Expired())
			{
				res.m_data = m_data;
				res.m_data.Catch();
			}

			return res;
		}

		auto operator<=>(const WeakRefPtr& ref) const noexcept
		{
			return m_data.Object <=> ref.m_data.Object;
		}

		auto operator<=>(std::nullopt_t) const noexcept
		{
			return m_data.Object <=> static_cast<RefObject*>(nullptr);
		}
	private:
		Internal::RefPtrData m_data;
	};
}
