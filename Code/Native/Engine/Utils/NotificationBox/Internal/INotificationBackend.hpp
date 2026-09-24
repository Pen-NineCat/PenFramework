// File /Native/Engine/Utils/NotificationBox/Internal/INotificationBackend.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../NotificationTypes.hpp"
#include <expected>
#include <vector>

namespace PenEngine::Internal
{
	/// @brief 后端 → 前端 的完成事件投递单元
	///
	/// @note **这是本设计的关键接缝**：平台回调可能在**任意线程**到达
	///       （WinToast 是 ClassicCom 回调线程、macOS 在主线程 runloop、Linux 在 dispatch 线程），
	///       而后端的完成队列由前端在**主线程** drain。
	///       `ObjectSignal` 无锁、`CoroutineScheduler` 单线程，因此后端**绝不能**直接 Emit——
	///       必须先入队（这正是 SDL3 "投递事件"所对应的那一半，见主文档 §6.6）
	///
	/// @note 本结构跨越线程边界，故只用值语义、不持有任何归属指针
	struct BackendCompletion
	{
		/// @brief 平台侧的通知 id（WinToast 的 INT64、Linux 的 U32，统一装进 U64）
		U64 PlatformId = 0;

		NotificationOutcome Outcome = NotificationOutcome::None;

		/// @brief Outcome == ActionInvoked 时有效
		/// @note 由**后端**负责把平台的动作下标/键解析成调用方提供的稳定 Id，
		///       前端因此完全不需要知道下标的存在（见主文档 §6.2①）
		String ActionId;

		/// @brief Outcome == TextReplied 时有效
		String Text;
	};

	/// @brief 平台通知后端接口
	///
	/// @note **线程纪律（调用方必须遵守）**：
	///       - Initialize / Capabilities / Show / Hide / Clear / DrainCompleted / Pump
	///         全部只在**前端所属线程（主线程）**调用
	///       - 只有平台回调线程会碰后端的"完成队列"，且仅通过内部加锁的入队路径
	class INotificationBackend
	{
	public:
		virtual ~INotificationBackend() noexcept = default;

		INotificationBackend(const INotificationBackend&) = delete;
		INotificationBackend& operator=(const INotificationBackend&) = delete;

		/// @brief 后端名（用于日志与诊断，如 "Win32/WinToast"）
		[[nodiscard]] virtual StringView Name() const noexcept = 0;

		/// @brief 初始化后端（AUMI / bundle 探测 / 会话总线连接等）
		/// @return 成功返回 `void`；失败返回 `std::unexpected(NotificationError)`
		virtual std::expected<void, NotificationError> Initialize(const NotificationBoxConfig& config) = 0;

		/// @brief 查询当前能力
		/// @note 允许**重复调用**并在运行期变化（Linux 的服务端能力可随守护进程重启而变），
		///       因此实现不应把它缓存成一次性结果
		[[nodiscard]] virtual NotificationCapabilities Capabilities() const noexcept = 0;

		/// @brief 投递一条通知
		/// @param request 平台中立的请求
		/// @return 成功返回平台侧 id；失败返回 `std::unexpected(NotificationError)`
		/// @note 用 `expected` 取代了原先"返回 0 表示失败 + out-param"的双通道约定：
		///       平台 id 的 0 值不再需要被征用为错误哨兵
		/// @note 实现必须自行处理"能力缺失 + Guarantee 等级"：
		///       Require 未满足时应**不投递**并返回 `RequireUnsupported`
		virtual std::expected<U64, NotificationError> Show(const NotificationRequest& request) = 0;

		/// @brief 收起一条已投递的通知
		virtual void Hide(U64 platformId) noexcept = 0;

		/// @brief 收起并清空本后端的所有通知
		/// @note 语义按平台能力为限：Linux 规范没有"清空全部"，只能逐个 Close
		virtual void Clear() noexcept = 0;

		/// @brief 取出后端回调线程投递进来的完成事件（**非阻塞**）
		/// @note 只在主线程调用；实现内部应加锁保护队列
		virtual void DrainCompleted(std::vector<BackendCompletion>& out) = 0;

		/// @brief 泵平台消息（主线程每帧调用）
		/// @note Windows 为空实现（WinRT 事件由系统投递到进程）；
		///       Linux 需要在这里做 libdbus 的 `dbus_connection_read_write_dispatch`
		///       或 sd-bus 的 `sd_bus_process`
		virtual void Pump() noexcept {}

	protected:
		INotificationBackend() noexcept = default;
	};
}
