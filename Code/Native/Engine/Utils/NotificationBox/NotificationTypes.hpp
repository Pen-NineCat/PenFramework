// File /Native/Engine/Utils/NotificationBox/NotificationTypes.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../../String/String.hpp"
#include <optional>
#include <vector>

namespace PenEngine
{
	/// @brief 通知句柄：不透明 U64，内部打包 { 槽位下标(低32) | 代际(高32) }
	///
	/// @note **不透明**：调用方不得解释 m_value 的位模式，只可用于比较、哈希与存储
	/// @note 代际的意义：槽位被回收再复用后，旧句柄的代际不再匹配，
	///       查询会得到 NotificationOutcome::Stale 而**不是别人的数据**
	///       —— 与 AGENTS.md §4.3 为 C++/C# 桥定下的 {index, generation} 句柄表同构
	class NotificationHandle
	{
	public:
		constexpr NotificationHandle() noexcept = default;

		[[nodiscard]] constexpr bool IsValid() const noexcept { return m_value != 0; }
		[[nodiscard]] constexpr U64 Value() const noexcept { return m_value; }

		[[nodiscard]] friend constexpr bool operator==(NotificationHandle lhs, NotificationHandle rhs) noexcept = default;

	private:
		friend class NotificationBox;

		explicit constexpr NotificationHandle(U64 value) noexcept : m_value(value) {}
		U64 m_value = 0;
	};

	/// @brief 通知的结束原因
	/// @note **平台覆盖不完整是常态**，不要假定每个取值在所有平台都会出现：
	///       - Failed  仅 Windows 有真失败回调（macOS/Linux 拿不到"是否真被看到"）
	///       - Expired 三平台语义不同（Windows 由 setExpiration 触发；
	///                 Linux 是 expire_timeout 到期；macOS 无此概念）
	///       - Dismissed macOS 仅在用户**显式点 Dismiss 按钮**时才有，划走横幅不触发
	enum class NotificationOutcome : U8
	{
		None = 0,       ///< 尚未结束
		Activated,      ///< 用户点击通知本体
		ActionInvoked,  ///< 用户点了某个动作按钮（见 NotificationResult::ActionId）
		TextReplied,    ///< 文本回复（见 NotificationResult::Text）；Linux 规范无此能力
		Dismissed,      ///< 用户关闭
		Expired,        ///< 超时过期
		Hidden,         ///< 被 Hide() 主动收起
		Failed,         ///< 投递失败（仅 Windows）
		Denied,         ///< 权限被拒 / 后端不可用（macOS 未授权、Linux 无守护进程）
		Unsupported,    ///< Guarantee::Require 的能力平台不支持，请求未被投递
		Stale,          ///< 句柄已过期（代际不匹配）
	};

	/// @brief 请求的保证等级（见主文档 §6.5）
	/// @note Prefer：尽力而为，能力缺失时静默降级，调用方无需关心
	///       Require：能力缺失时**不投递**，Show() 失败，调用方自行换渠道（如退回模态框）
	enum class NotificationGuarantee : U8
	{
		Prefer = 0,
		Require,
	};

	/// @brief 打断强度（四档，取自跨平台先例的准共识，见主文档 §6.4 / §7.4）
	/// @note 这是"打断强度"轴，**不是**"语义场景"轴。Windows 的 Scenario 是语义场景，
	///       二者不可一一映射，故本层不暴露 Scenario
	enum class NotificationUrgency : U8
	{
		Low = 0,
		Normal,
		High,
		Critical,
	};

	/// @brief 一个动作按钮
	/// @note Id 是**调用方提供的稳定标识**，回调按它返回，而非下标
	///       —— Windows 后端内部维护 Id ↔ WinToast actionIndex 的映射（见主文档 §6.2①）
	/// @note Id **不得**为 "default"：Linux 规范把 "default" 保留给"点击通知本体"
	struct NotificationAction
	{
		String Id;
		String Label;
	};

	/// @brief 通知请求
	struct NotificationRequest
	{
		String Title;
		String Body;

		std::vector<NotificationAction> Actions;

		/// @brief 是否需要文本输入框（回复）
		/// @note Linux 规范**完全没有**此能力；Windows 经 WinToast 的 addInput() 提供，
		///       但其占位符与按钮文案是**硬编码**的，且**不能与 Actions 共存**
		///       （WinToast 会生成两个 <actions> 节点，而 schema 只允许一个）
		bool TextReply = false;

		/// @brief 图标 / 大图，**绝对文件路径**，不带 scheme
		/// @note 三平台要求不同（见主文档 §7.7）：Windows 会被无条件前置 file:///；
		///       macOS 必须落盘且 ≤10MB；Linux 接受 file:// URI 或主题名
		String ImagePath;

		/// @brief Hero 大图（顶部大图）
		/// @note 仅 Windows（build ≥ 14393）有此概念；macOS / Linux 无
		String HeroImagePath;

		/// @brief 圆形裁剪
		/// @note 仅 Windows（build ≥ 14393）；macOS 只有任意矩形裁剪
		bool CircleCrop = false;

		/// @brief 归因文本（"via X"小字）
		/// @note 仅 Windows（主版本 > 6）有此概念
		String Attribution;

		NotificationUrgency Urgency = NotificationUrgency::Normal;

		/// @brief 横幅停留时长（毫秒）。0 = 平台默认
		/// @note 与 ExpirationMs **不是一回事**（见主文档 §7.5）
		U64 TimeoutMs = 0;

		/// @brief 到期后从通知中心移除（毫秒）。0 = 不设置
		/// @note 仅 Windows 有原生支持；macOS/Linux 需自行计时
		U64 ExpirationMs = 0;

		/// @brief 静音
		bool Silent = false;

		/// @brief 应用在前台时是否也要显示
		/// @note **macOS 前台默认不显示**，必须由后端实现 willPresent 才会出现（见主文档 §7.8）。
		///       对通常在前台运行的游戏/编辑器，此默认值很关键
		bool ShowWhenForeground = true;

		NotificationGuarantee ActionsGuarantee = NotificationGuarantee::Prefer;
		NotificationGuarantee TextReplyGuarantee = NotificationGuarantee::Prefer;

		/// @brief 调用方自定义的不透明标签，会被原样回传
		/// @note 用 U64 而非指针：广播 payload 里的指针会在发起者先死时悬垂（见主文档 §6.6）
		U64 UserTag = 0;
	};

	/// @brief 通知结束后的结果
	struct NotificationResult
	{
		NotificationHandle Handle;
		U64 UserTag = 0;
		NotificationOutcome Outcome = NotificationOutcome::None;

		/// @brief Outcome == ActionInvoked 时有效
		String ActionId;

		/// @brief Outcome == TextReplied 时有效
		String Text;
	};

	/// @brief 后端能力查询结果
	/// @note 必须是**可重复查询**的：Linux 的能力由服务端在运行时返回，
	///       会随守护进程重启而变；Windows/macOS 是启动期确定（见主文档 §5.3）
	struct NotificationCapabilities
	{
		bool Actions = false;
		bool TextReply = false;
		bool Image = false;
		bool HeroImage = false;
		bool CircleCrop = false;
		bool Attribution = false;
		bool Sound = false;
		bool CustomSound = false;
		bool LoopSound = false;
		bool Timeout = false;
		bool Expiration = false;
		bool Grouping = false;
		bool Badge = false;
		bool Scheduling = false;
		bool Query = false;
		bool Replace = false;
		bool DismissCallback = false;
		bool FailedCallback = false;

		/// @brief 本平台后端当前是否真的可用
		/// @note macOS 非 bundle 进程为 false（取单例即终止进程，见主文档 §3.1）
		bool Available = false;
	};

	/// @brief 错误码
	/// @note **不要**转手调用 WinToast::strerror：1.3.2 的查表缺 InvalidHandler，
	///       且带 assert，Debug 下会断言中断（见主文档 §2.3 D7）
	enum class NotificationError : U8
	{
		NoError = 0,
		NotInitialized,
		SystemNotSupported,
		BackendUnavailable,   ///< macOS 非 bundle / Linux 无守护进程 / 权限被拒
		InvalidConfig,
		InvalidRequest,
		InvalidHandler,
		NotDisplayed,
		RequireUnsupported,   ///< Require 等级的能力平台不支持
		Unknown,
	};

	[[nodiscard]] StringView ToString(NotificationError error) noexcept;
	[[nodiscard]] StringView ToString(NotificationOutcome outcome) noexcept;

	/// @brief 初始化配置（平台中立）
	/// @note Windows 后端会把 Organization/Application/Version 映射为 AUMI 的前三段
	///       （WinToast 的第四段 versionInformation 由 Version 填充）
	struct NotificationBoxConfig
	{
		String Organization;
		String Application;
		String Version;

		/// @brief 是否允许与系统外壳集成
		/// @note **各平台含义不同**：
		///       - Windows：是否在开始菜单写入 `<Application>.lnk`，以此把 AUMI 注册给外壳。
		///         ⚠️ **实测（见调研文档 §2.3.3）：动作按钮能否渲染，取决于 AUMI 是否经该
		///         `.lnk` 注册。** 不注册时通知**气泡可以正常显示**，但**不显示任何动作按钮**；
		///         仅由 `initialize()` 间接调用 `SetCurrentProcessExplicitAppUserModelID`
		///         并不足以渲染交互元素。因此**需要动作按钮的调用方必须开启本项**。
		///         代价：持久改变用户系统（写入 `.lnk`），且失败会使 `Initialize()` 失败。
		///         默认 **false**——库本身不产生副作用，由调用方显式选择
		///       - macOS：通知要求 .app bundle；此开关无法改变该事实，非 bundle 时后端报 BackendUnavailable
		///       - Linux：忽略
		bool AllowShellRegistration = false;

		/// @brief 应用图标绝对路径（可选）
		String IconPath;
	};
}
