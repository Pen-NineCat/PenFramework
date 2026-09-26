// File /Native/Engine/Utils/NotificationBox/NotificationBox.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../../Object/SignalObject.hpp"
#include "../../Coroutine/CoroutineTask.hpp"
#include "../../Utils/OwnerThread.hpp"
#include "../../Utils/Singleton.hpp"
#include "NotificationTypes.hpp"

// 必须包含完整定义：m_completionScratch 是 std::vector<Internal::BackendCompletion>，
// 而 vector 的元素类型必须完整
#include "Internal/INotificationBackend.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace PenEngine
{
	/// @brief 平台中立的通知盒
	///
	/// 设计要点（详见 `PersonalWorkaround/NotificationBox-Research/NotificationBox-跨平台能力并集分析.md`）：
	///
	/// - **门铃 + 句柄**：`FinishedSignal` **只表示"有通知结束了"**，不携带结果。
	///   权威数据存在句柄槽位里，双方都靠 `TryGetResult(handle)` 查询。
	///   之所以让信号不带 payload：`SignalAwaitable` 只保留**最后一次** Emit 的参数，
	///   而调度器**每帧只轮询一次** `IsReady()`——同帧多条通知结束时，
	///   前一条的数据会被后一条覆盖，等待它的协程将**永久挂起**。
	///   让信号无 payload 就从根本上消除了这个不对称。
	/// - **句柄而非指针**：对外只有不透明 `NotificationHandle`（U64 + 代际）。
	///   盒**不持有任何监听者指针**，因此不存在监听者先死的悬垂问题。
	/// - **回调统一投递回主线程**：后端回调线程只入队，`Update()` 在主线程 drain。
	///
	/// @note 本类**只依赖信号机制**，因此基类含 `SignalObject` 而非 `PObject`：
	///       通知盒不需要反射、属性槽与延迟销毁。协程的**提交**仍由调用方完成
	///       （`PObject::StartCoroutine` / `CoroutineScheduler::PostCoroutineTask` 都需要
	///       一个 `PObject*` sender），本类只负责**产出** `CoroutineTask`
	///
	/// @note 同时是**单例**：通知后端在平台侧是进程级唯一的
	///       （Windows 的 `WinToast::instance()` 甚至是 **thread_local** 单例，
	///       多个实例会互相覆盖 AUMI / AppName / 快捷方式策略；
	///       Linux 的会话总线连接也应当只建一条）。
	///       用法：`NotificationBox::GetInstance()`
	/// @note 基类顺序与 `CoroutineScheduler` 保持一致（`Singleton<T>` 在前、信号基类在后）
	class NotificationBox final : public Singleton<NotificationBox>, public SignalObject
	{
	public:
		using Backend = Internal::INotificationBackend;

		NotificationBox() noexcept;
		virtual ~NotificationBox() noexcept override;

		NotificationBox(const NotificationBox&) = delete;
		NotificationBox& operator=(const NotificationBox&) = delete;
		NotificationBox(NotificationBox&&) = delete;
		NotificationBox& operator=(NotificationBox&&) = delete;

		// ---- 生命周期 ----

		/// @brief 初始化后端
		/// @note 记录本对象所属线程；此后所有方法都要求在该线程调用
		/// @return 成功返回 `void`；失败返回 `std::unexpected(NotificationError)`
		/// @note 改成 `expected` 是为与本类 `Show` 统一（原先用 out-param `NotificationError*`）
		std::expected<void, NotificationError> Initialize(const NotificationBoxConfig& config);

		[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
		[[nodiscard]] bool IsOwnerThread() const noexcept { return m_ownerThread.IsOwner(); }

		/// @brief 查询当前能力（可重复调用；Linux 侧结果可能随守护进程重启而变）
		[[nodiscard]] NotificationCapabilities Capabilities() const noexcept;

		// ---- 投递 ----

		/// @brief 投递一条通知
		/// @return 成功返回句柄；失败返回 `std::unexpected(NotificationError)`
		/// @note 用 `expected` 而非 `optional` + out-param：与本仓库既有风格一致
		///       （`PObject::TryGetProperty` 同样返回 `std::expected`），
		///       且失败原因不可能被调用方忽略
		std::expected<NotificationHandle, NotificationError> Show(const NotificationRequest& request);

		/// @brief 收起一条通知（仍会产生 Outcome::Hidden 的完成事件）
		bool Hide(NotificationHandle handle);

		/// @brief 收起并清空全部
		/// @note 不标 noexcept：要把在途槽位了结为 Hidden 需要入队分配
		void Clear();

		// ---- 主循环钩子 ----

		/// @brief 主线程每帧调用：泵平台消息 → drain 后端完成队列 → 写槽位 → 响一次门铃
		/// @note 挂到 `CoreApplication::Exec()` 的主循环（泵窗口消息之后）
		void Update();

		// ---- 结果查询（权威路径）----

		/// @brief 查询句柄的结果
		/// @return 尚未结束时返回 nullopt；
		///         已结束返回结果；
		///         句柄陈旧/无效时返回 `Outcome == NotificationOutcome::Stale` 的结果
		///         （**不返回 nullopt**，否则等待者会陷入死循环）
		/// @note 非破坏性：可被多个监听者与协程重复查询，直到 `Release()`
		[[nodiscard]] std::optional<NotificationResult> TryGetResult(NotificationHandle handle) const;

		/// @brief 释放句柄槽位（结果与代际一并失效）
		/// @note 不释放则槽位一直占用；Stale 检测正是靠"回收时自增代际"
		void Release(NotificationHandle handle) noexcept;

		/// @brief 帧内完成的句柄快照（供监听者廉价过滤）
		/// @note 这是**状态快照**而非瞬时事件，因此在两份 `Update()` 之间重复读取是安全的
		[[nodiscard]] std::span<const NotificationHandle> CompletedSinceLastUpdate() const noexcept
		{
			return m_completedThisUpdate;
		}

		// ---- 等待 ----

		/// @brief 等待某条通知结束
		/// @note 实现是"门铃 + 重查谓词"：醒来后回查句柄，绝不信任信号参数
		[[nodiscard]] CoroutineTask<NotificationResult> WaitFinished(NotificationHandle handle);

		// ---- 信号 ----

		/// @brief 门铃：**不带 payload**，只表示"有通知结束了"。收到后请自行 `TryGetResult`
		/// @note 监听者用 `SignalObject::Connect(FinishedSignal, f)` 注册（`PObject` 亦可，
		///       它派生自 `SignalObject`）；槽位随监听者析构自动断开
		ObjectSignal<> FinishedSignal{ this };

	private:
		/// @brief 一个句柄槽位
		struct Slot
		{
			U32 Generation = 0;
			bool Occupied = false;
			bool Completed = false;
			U64 PlatformId = 0;
			U64 UserTag = 0;
			NotificationResult Result;
		};

		// ---- 句柄打包（不透明 U64：低 32 = 下标，高 32 = 代际）----
		[[nodiscard]] static constexpr U32 IndexOf(NotificationHandle handle) noexcept
		{
			return static_cast<U32>(handle.m_value & 0xFFFFFFFFull);
		}
		[[nodiscard]] static constexpr U32 GenerationOf(NotificationHandle handle) noexcept
		{
			return static_cast<U32>(handle.m_value >> 32);
		}
		[[nodiscard]] static constexpr NotificationHandle MakeHandle(U32 index, U32 generation) noexcept
		{
			// 代际从 1 起，保证默认构造的句柄（0）必然无效
			return NotificationHandle((static_cast<U64>(generation) << 32) | index);
		}

		/// @brief 占用一个槽位（代际自增），返回有效句柄
		/// @note `I3`：必须在调用后端 `Show` **之前**占位，失败时回滚
		NotificationHandle AcquireSlot(U64 userTag);
		void RollbackSlot(NotificationHandle handle) noexcept;
		void BindPlatformId(NotificationHandle handle, U64 platformId) noexcept;

		[[nodiscard]] Slot* FindSlot(NotificationHandle handle) noexcept;
		[[nodiscard]] const Slot* FindSlot(NotificationHandle handle) const noexcept;
		[[nodiscard]] Slot* FindSlotByPlatformId(U64 platformId) noexcept;

		void DrainBackend();

		std::unique_ptr<Backend> m_backend;
		std::vector<Slot> m_slots;
		std::vector<U32> m_freeIndices;

		/// @brief 帧内完成快照；只在 `Update()` 中重写（I4）
		std::vector<NotificationHandle> m_completedThisUpdate;

		/// @brief 待发布批次：由 DrainBackend / Clear 追加，Update 时整体移入快照并响门铃
		/// @note 之所以要两个容器：Clear() 可能发生在 Update 之外，
		///       其产生的完成必须留到下一次 Update 才响门铃（否则门铃会在无观察者时白响）
		std::vector<NotificationHandle> m_completedPending;

		/// @brief drain 复用缓冲，避免每帧分配
		std::vector<Internal::BackendCompletion> m_completionScratch;

		/// @brief 归属线程（I1）：构造即绑定，`Initialize()` 会再绑定一次
		OwnerThread m_ownerThread;
		bool m_initialized = false;
		bool m_updating = false;
	};

	// ================================================================
	// 不变量（改本类实现前先读，改完同步更新）
	//
	//   I1 只有 m_ownerThread 线程访问 m_slots / m_freeIndices / 门铃；
	//      Show / Update / Hide / Release / WaitFinished 必须同线程
	//   I2 后端回调线程**只入队**（经后端内部加锁的队列），绝不触碰 m_slots。
	//      这是"后端回调不得直接 Emit"的落地形式
	//   I3 完成事件只在 Update() 中写入槽位，且此时 platformId 必已绑定：
	//      Show 先占槽位→再调后端→再绑 id，而完成队列只在主线程 drain，
	//      故不存在"完成早于绑定"的窗口
	//   I4 门铃每次 Update **最多 Emit 一次**，且必在所有槽位写完之后
	//   I5 槽位回收只经 Release()；占用时代际自增，故旧句柄一律得到 Stale 而不会读到他人数据
	// ================================================================

	inline CoroutineTask<NotificationResult> NotificationBox::WaitFinished(NotificationHandle handle)
	{
		while (true)
		{
			if (auto result = TryGetResult(handle))
				co_return std::move(*result);

			// 门铃：醒来后回到循环顶部重查句柄。
			// 这里**故意不使用**信号参数——信号本来就没有 payload
			co_await FinishedSignal;
		}
	}
}
