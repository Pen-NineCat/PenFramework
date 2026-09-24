// File /Native/Engine/Utils/NotificationBox/NotificationBox.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "NotificationBox.hpp"
#include "Internal/INotificationBackend.hpp"
#include "../../DebugTools/DebugVerify.hpp"

#ifdef PENFRAMEWORK_OS_WIN32
#include "Win32NotificationBox.h"
#endif

namespace PenEngine
{
	// ================================================================
	// 错误码 / 结束原因的字符串化
	//   ⚠️ 刻意**不复用** WinToast::strerror：1.3.2 的查表缺 InvalidHandler，
	//      且带 assert(iter != Labels.end())，Debug 下传它会断言中断（主文档 §2.3 D7）
	// ================================================================

	StringView ToString(NotificationError error) noexcept
	{
		switch (error)
		{
			case NotificationError::NoError:            return "NoError";
			case NotificationError::NotInitialized:     return "NotInitialized";
			case NotificationError::SystemNotSupported: return "SystemNotSupported";
			case NotificationError::BackendUnavailable: return "BackendUnavailable";
			case NotificationError::InvalidConfig:      return "InvalidConfig";
			case NotificationError::InvalidRequest:     return "InvalidRequest";
			case NotificationError::InvalidHandler:     return "InvalidHandler";
			case NotificationError::NotDisplayed:       return "NotDisplayed";
			case NotificationError::RequireUnsupported: return "RequireUnsupported";
			case NotificationError::Unknown:            return "Unknown";
		}
		return "Unknown";
	}

	StringView ToString(NotificationOutcome outcome) noexcept
	{
		switch (outcome)
		{
			case NotificationOutcome::None:          return "None";
			case NotificationOutcome::Activated:     return "Activated";
			case NotificationOutcome::ActionInvoked: return "ActionInvoked";
			case NotificationOutcome::TextReplied:   return "TextReplied";
			case NotificationOutcome::Dismissed:     return "Dismissed";
			case NotificationOutcome::Expired:       return "Expired";
			case NotificationOutcome::Hidden:        return "Hidden";
			case NotificationOutcome::Failed:        return "Failed";
			case NotificationOutcome::Denied:        return "Denied";
			case NotificationOutcome::Unsupported:   return "Unsupported";
			case NotificationOutcome::Stale:         return "Stale";
		}
		return "None";
	}

	namespace
	{
		/// @brief 平台后端工厂
		/// @note macOS / Linux 后端尚未实现：它们各自需要 .mm / libdbus，
		///       且分别受 §9 Q6（bundle）与 Q5（依赖选型）阻塞
		std::unique_ptr<Internal::INotificationBackend> CreatePlatformBackend()
		{
			#ifdef PENFRAMEWORK_OS_WIN32
			return std::make_unique<Win32NotificationBox>();
			#else
			return nullptr;
			#endif
		}

		/// @brief 该结束原因是否代表"用户与通知发生了交互"
		/// @note 交互结果在槽位里是**粘性**的：一条通知可能先报交互、后报生命周期
		///       （点了按钮 → ActionInvoked，系统随即收起 → Dismissed）。
		///       若允许后者覆盖，调用方将永远拿不到 ActionId / Text
		[[nodiscard]] constexpr bool IsInteractionOutcome(NotificationOutcome outcome) noexcept
		{
			return outcome == NotificationOutcome::Activated
				|| outcome == NotificationOutcome::ActionInvoked
				|| outcome == NotificationOutcome::TextReplied;
		}
	}

	NotificationBox::NotificationBox() noexcept
		: m_ownerThread(std::this_thread::get_id())
	{
	}

	NotificationBox::~NotificationBox() noexcept
	{
		// 先收起全部，避免后端销毁后平台侧仍有悬挂的通知；
		// 同时 Clean 掉槽位，避免完成事件在析构后回访
		if (m_backend)
		{
			m_backend->Clear();
			m_backend.reset();
		}

		m_slots.clear();
		m_freeIndices.clear();
	}

	// ================================================================
	// 生命周期
	// ================================================================

	std::expected<void, NotificationError> NotificationBox::Initialize(const NotificationBoxConfig& config)
	{
		if (m_initialized)
		{
			// 重复初始化按失败处理，避免后端被悄悄替换
			return std::unexpected(NotificationError::InvalidConfig);
		}

		m_ownerThread = std::this_thread::get_id();

		m_backend = CreatePlatformBackend();
		if (!m_backend)
		{
			// macOS 非 bundle / 未实现的平台都走这里
			return std::unexpected(NotificationError::BackendUnavailable);
		}

		// 后端的 expected 直接向上传递，不做转换
		if (auto initialized = m_backend->Initialize(config); !initialized)
			return initialized;

		m_initialized = true;
		return {};
	}

	NotificationCapabilities NotificationBox::Capabilities() const noexcept
	{
		if (!m_backend)
			return NotificationCapabilities{};

		// 刻意不缓存：Linux 侧能力由服务端在运行时返回，可能变化（主文档 §5.3）
		return m_backend->Capabilities();
	}

	// ================================================================
	// 槽位与句柄
	// ================================================================

	NotificationHandle NotificationBox::AcquireSlot(U64 userTag)
	{
		U32 index = 0;

		if (!m_freeIndices.empty())
		{
			index = m_freeIndices.back();
			m_freeIndices.pop_back();
		}
		else
		{
			index = static_cast<U32>(m_slots.size());
			m_slots.emplace_back();
		}

		Slot& slot = m_slots[index];

		// 代际自增：回收复用后旧句柄必然失配（I5）。
		// 跳过 0，保证任何有效句柄的 m_value 都不为 0
		if (++slot.Generation == 0)
			++slot.Generation;

		slot.Occupied = true;
		slot.Completed = false;
		slot.PlatformId = 0;
		slot.UserTag = userTag;
		slot.Result = NotificationResult{};

		const NotificationHandle handle = MakeHandle(index, slot.Generation);
		slot.Result.Handle = handle;
		slot.Result.UserTag = userTag;
		return handle;
	}

	void NotificationBox::RollbackSlot(NotificationHandle handle) noexcept
	{
		Slot* slot = FindSlot(handle);
		if (slot == nullptr)
			return;

		slot->Occupied = false;
		slot->Completed = false;
		slot->PlatformId = 0;
		m_freeIndices.push_back(IndexOf(handle));
	}

	void NotificationBox::BindPlatformId(NotificationHandle handle, U64 platformId) noexcept
	{
		if (Slot* slot = FindSlot(handle))
			slot->PlatformId = platformId;
	}

	NotificationBox::Slot* NotificationBox::FindSlot(NotificationHandle handle) noexcept
	{
		if (!handle.IsValid())
			return nullptr;

		const U32 index = IndexOf(handle);
		if (index >= m_slots.size())
			return nullptr;

		Slot& slot = m_slots[index];
		if (!slot.Occupied || slot.Generation != GenerationOf(handle))
			return nullptr;

		return &slot;
	}

	const NotificationBox::Slot* NotificationBox::FindSlot(NotificationHandle handle) const noexcept
	{
		return const_cast<NotificationBox*>(this)->FindSlot(handle);
	}

	NotificationBox::Slot* NotificationBox::FindSlotByPlatformId(U64 platformId) noexcept
	{
		for (Slot& slot : m_slots)
		{
			// 刻意**不**排除已完成的槽位：同一条通知可能先后产生多个事件
			// （典型：用户点了动作按钮 → ActionInvoked，随后系统收起 → Dismissed），
			// 是否覆盖由 DrainBackend 的粘性规则决定
			if (slot.Occupied && slot.PlatformId == platformId)
				return &slot;
		}
		return nullptr;
	}

	// ================================================================
	// 投递
	// ================================================================

	std::expected<NotificationHandle, NotificationError> NotificationBox::Show(const NotificationRequest& request)
	{
		DEBUG_VERIFY_REPORT(IsOwnerThread(), "NotificationBox::Show must run on the owner thread");

		if (!m_initialized)
			return std::unexpected(NotificationError::NotInitialized);

		// ---- 请求校验（跨平台一致的部分）----
		if (request.Title.Empty() && request.Body.Empty())
			return std::unexpected(NotificationError::InvalidRequest);

		for (const NotificationAction& action : request.Actions)
		{
			// Id 是回调的载体，必须非空
			if (action.Id.Empty() || action.Label.Empty())
				return std::unexpected(NotificationError::InvalidRequest);

			// "default" 被 Linux 规范保留给"点击通知本体"（见 NotificationTypes.hpp）
			if (action.Id == "default")
				return std::unexpected(NotificationError::InvalidRequest);

			// 同一通知内 Id 不得重复
			for (const NotificationAction& other : request.Actions)
			{
				if (&other != &action && other.Id == action.Id)
					return std::unexpected(NotificationError::InvalidRequest);
			}
		}

		// Windows 经 WinToast 时 Actions 与 TextReply **互斥**
		//（addAction + addInput 会生成两个 <actions> 节点，而 schema 只允许一个，
		//  见主文档 §2.3 D2）。两端都要求 Require 时只能拒绝
		const bool bothRequired = request.TextReply
			&& request.TextReplyGuarantee == NotificationGuarantee::Require
			&& !request.Actions.empty()
			&& request.ActionsGuarantee == NotificationGuarantee::Require;

		if (bothRequired)
			return std::unexpected(NotificationError::RequireUnsupported);

		// ---- I3：先占槽位，再调后端；失败回滚 ----
		const NotificationHandle handle = AcquireSlot(request.UserTag);

		std::expected<U64, NotificationError> platformId = m_backend->Show(request);
		if (!platformId)
		{
			RollbackSlot(handle);
			return std::unexpected(platformId.error());
		}

		BindPlatformId(handle, *platformId);
		return handle;
	}

	bool NotificationBox::Hide(NotificationHandle handle)
	{
		DEBUG_VERIFY_REPORT(IsOwnerThread(), "NotificationBox::Hide must run on the owner thread");

		Slot* slot = FindSlot(handle);
		if (slot == nullptr || slot->PlatformId == 0)
			return false;

		if (!m_initialized)
			return false;

		m_backend->Hide(slot->PlatformId);
		return true;
	}

	void NotificationBox::Clear()
	{
		if (!m_backend)
			return;

		m_backend->Clear();

		// Windows 的 WinToast::clear() 摘掉 token 与缓冲，**不会**回调，
		// 因此这里要主动把仍在途的槽位了结为 Hidden，否则调用者会永久等待
		for (Slot& slot : m_slots)
		{
			if (!slot.Occupied || slot.Completed)
				continue;

			slot.Completed = true;
			slot.Result.Outcome = NotificationOutcome::Hidden;
			m_completedPending.push_back(slot.Result.Handle);
		}
	}

	// ================================================================
	// 主循环钩子
	// ================================================================

	void NotificationBox::DrainBackend()
	{
		m_completionScratch.clear();
		m_backend->DrainCompleted(m_completionScratch);

		for (const Internal::BackendCompletion& completion : m_completionScratch)
		{
			Slot* slot = FindSlotByPlatformId(completion.PlatformId);
			if (slot == nullptr)
			{
				// 已被 Release 的通知，或后端误报了未知 id：丢弃
				continue;
			}

			// 交互结果**粘性**：用户点过按钮之后，系统通常还会补一次 Dismissed。
			// 若允许它覆盖，调用方就再也拿不到 ActionId / Text 了——
			// 所以"已是交互结果"的槽位只接受新的交互结果，不接受生命周期结果
			if (slot->Completed
				&& IsInteractionOutcome(slot->Result.Outcome)
				&& !IsInteractionOutcome(completion.Outcome))
			{
				continue;
			}

			slot->Completed = true;
			slot->Result.Outcome = completion.Outcome;
			slot->Result.ActionId = completion.ActionId;
			slot->Result.Text = completion.Text;

			m_completedPending.push_back(slot->Result.Handle);
		}
	}

	void NotificationBox::Update()
	{
		DEBUG_VERIFY_REPORT(IsOwnerThread(), "NotificationBox::Update must run on the owner thread");

		// Update 不允许重入：重入会让"本帧批次"的语义变得不可解释
		DEBUG_VERIFY_REPORT(!m_updating, "NotificationBox::Update must not be re-entered");

		if (!m_initialized)
			return;

		m_updating = true;

		// 1. 泵平台消息（Windows 空实现；Linux 在这里做 dbus dispatch）
		m_backend->Pump();

		// 2. drain 后端完成队列 → 写槽位（后端回调线程只入队，见 I2）
		DrainBackend();

		// 3. 发布本帧批次 + 响一次门铃（I4：最多一次，且必在写完之后）
		if (!m_completedPending.empty())
		{
			m_completedThisUpdate.swap(m_completedPending);
			m_completedPending.clear();
			FinishedSignal.Emit();
		}

		m_updating = false;
	}

	// ================================================================
	// 结果查询 / 释放
	// ================================================================

	std::optional<NotificationResult> NotificationBox::TryGetResult(NotificationHandle handle) const
	{
		// 陈旧或无效句柄：**返回 Stale 结果而非 nullopt**，
		// 否则 WaitFinished 的"门铃 + 重查"循环会永远转下去
		if (const Slot* slot = FindSlot(handle))
		{
			if (!slot->Completed)
				return std::nullopt;

			return slot->Result;
		}

		NotificationResult stale;
		stale.Handle = handle;
		stale.Outcome = NotificationOutcome::Stale;
		return stale;
	}

	void NotificationBox::Release(NotificationHandle handle) noexcept
	{
		DEBUG_VERIFY_REPORT(IsOwnerThread(), "NotificationBox::Release must run on the owner thread");
		RollbackSlot(handle);
	}
}
