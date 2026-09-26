// File /Native/Engine/Utils/NotificationBox/Win32NotificationBox.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Win32NotificationBox.h"

#include "../../DebugTools/DebugVerify.hpp"
#include "../../OS/Windows/Windows.h"

// WinToast 只在 .cpp 里出现（见头文件注释）
#include <wintoastlib.h>

#include <climits>
#include <objbase.h>
#include <string>

namespace PenEngine
{
	namespace
	{
		/// @brief 把 WinToast 的 error 映射为本层的 error
		/// @note **刻意不使用 `WinToast::strerror`**：1.3.2 的查表缺 `InvalidHandler` 项，
		///       且带 `assert(iter != Labels.end())`，Debug 下传它会断言中断（主文档 §2.3 D7）
		NotificationError MapError(WinToastLib::WinToast::WinToastError error) noexcept
		{
			using WinToastError = WinToastLib::WinToast::WinToastError;

			switch (error)
			{
				case WinToastError::NoError:               return NotificationError::NoError;
				case WinToastError::NotInitialized:        return NotificationError::NotInitialized;
				case WinToastError::SystemNotSupported:    return NotificationError::SystemNotSupported;
				case WinToastError::ShellLinkNotCreated:   return NotificationError::BackendUnavailable;
				case WinToastError::InvalidAppUserModelID: return NotificationError::InvalidConfig;
				case WinToastError::InvalidParameters:     return NotificationError::InvalidConfig;
				case WinToastError::InvalidHandler:        return NotificationError::InvalidHandler;
				case WinToastError::NotDisplayed:          return NotificationError::NotDisplayed;
				case WinToastError::UnknownError:          return NotificationError::Unknown;
			}
			return NotificationError::Unknown;
		}

		/// @brief String(UTF-8) → std::wstring
		/// @note 走 `ConvertToStdString`，即 boost::locale 的 UTF 转换；
		///       `BasicString` **没有** 跨字符类型的构造函数，不能直接 `std::wstring(str)`
		[[nodiscard]] std::wstring ToWide(const String& text)
		{
			return text.ConvertToStdString<wchar_t>();
		}

		/// @brief std::wstring → String(UTF-8)
		[[nodiscard]] String FromWide(const std::wstring& text)
		{
			String result;
			result.ConvertFrom(text);
			return result;
		}
	}

	// ================================================================
	// WinToast 回调处理器
	//
	//   WinToast 以 `std::shared_ptr<IWinToastHandler>` **接管所有权并会 delete**
	//   （`wintoastlib.cpp:721`），因此必须 `new` 出来，调用方不得再碰它（主文档 §2.3 D3）
	//
	//   本处理器**只入队，不做任何其它事**：回调线程上不能 Emit 信号，
	//   也不能触碰前端槽位（主文档 §6.6 与 I2）
	// ================================================================
	class Win32ToastHandler final : public WinToastLib::IWinToastHandler
	{
	public:
		Win32ToastHandler(std::shared_ptr<Win32NotificationBox::CompletionQueue> queue,
			std::shared_ptr<Win32NotificationBox::Record> record) noexcept
			: m_queue(std::move(queue)), m_record(std::move(record))
		{
		}

		void toastActivated() const override
		{
			Push(NotificationOutcome::Activated, String(), String());
		}

		/// @brief 动作按钮被点击
		/// @note WinToast 传回的是**下标**（`addAction` 时以 `"%zd" % index` 作为 arguments，
		///       见 `addActionHelper`），这里立刻还原成调用方提供的稳定 Id：
		///       前端因此永远不需要知道下标的存在（主文档 §6.2①）
		void toastActivated(int actionIndex) const override
		{
			String actionId;
			if (actionIndex >= 0 && static_cast<Usize>(actionIndex) < m_record->ActionIds.size())
				actionId = m_record->ActionIds[static_cast<Usize>(actionIndex)];

			Push(NotificationOutcome::ActionInvoked, actionId, String());
		}

		/// @brief 文本输入框被提交
		/// @note 走这条路径意味着本通知用 WinToast 的 `addInput()`，
		///       其占位符与按钮文案是**硬编码**的（`"..."` / `"Reply"`），无法自定义
		void toastActivated(std::wstring response) const override
		{
			Push(NotificationOutcome::TextReplied, String(), FromWide(response));
		}

		void toastDismissed(WinToastDismissalReason state) const override
		{
			NotificationOutcome outcome = NotificationOutcome::Dismissed;

			switch (state)
			{
				case WinToastDismissalReason::UserCanceled:      outcome = NotificationOutcome::Dismissed; break;
				case WinToastDismissalReason::ApplicationHidden: outcome = NotificationOutcome::Hidden;    break;
				case WinToastDismissalReason::TimedOut:          outcome = NotificationOutcome::Expired;   break;
			}

			// 注：WinToast 会把"设了 setExpiration 且已过期"的 UserCanceled 改写成 TimedOut
			Push(outcome, String(), String());
		}

		void toastFailed() const override
		{
			Push(NotificationOutcome::Failed, String(), String());
		}

	private:
		void Push(NotificationOutcome outcome, const String& actionId, const String& text) const
		{
			auto item = Win32NotificationBox::QueuedCompletion{};
			item.Owner = m_record;
			item.Outcome = outcome;
			item.ActionId = actionId;
			item.Text = text;

			// 仅入队。锁只保护队列本身（I6）
			const std::lock_guard<std::mutex> guard(m_queue->Mutex);
			m_queue->Items.push_back(std::move(item));
		}

		std::shared_ptr<Win32NotificationBox::CompletionQueue> m_queue;
		std::shared_ptr<Win32NotificationBox::Record> m_record;
	};

	// ================================================================
	// 后端实现
	// ================================================================

	Win32NotificationBox::Win32NotificationBox() noexcept
	{
	}

	Win32NotificationBox::~Win32NotificationBox() noexcept
	{
		// 清空完成队列：此时不应再有任何通知需要上报
		if (m_queue)
		{
			const std::lock_guard<std::mutex> guard(m_queue->Mutex);
			m_queue->Items.clear();
		}

		// 与 Initialize 中的 CoInitializeEx 配对（S_FALSE 也算一次占用）
		if (m_comInitialized)
			CoUninitialize();
	}

	StringView Win32NotificationBox::Name() const noexcept
	{
		return "Win32/WinToast";
	}

	bool Win32NotificationBox::IsAcceptableImagePath(StringView path) noexcept
	{
		// WinToast 的 setImageFieldHelper 会**无条件**前置 "file:///"（wintoastlib.cpp:1096），
		// 所以这里必须是"纯绝对路径"，不能带 scheme
		if (path.Empty() || path.Find("://") != StringView::NPos)
			return false;

		// Debug 下 WinToast 自带 assert(path.size() < MAX_PATH)，宁可在此提前拒绝
		if (path.Size() >= MAX_PATH)
			return false;

		// 形如 "C:/..." 或 "C:\..."（盘符 + 冒号）
		return path.Size() >= 3 && path[1] == ':';
	}

	std::expected<void, NotificationError> Win32NotificationBox::Initialize(const NotificationBoxConfig& config)
	{
		m_ownerThread.Bind();

		using WinToast = WinToastLib::WinToast;

		// ---- COM 初始化（必须做，且必须早于任何 WinRT 调用）----
		//
		// ⚠️ WinToast **不会**替我们初始化 COM：`initialize()` 只在
		//    `_shortcutPolicy != SHORTCUT_POLICY_IGNORE` 时经由 `createShortcut()` 调
		//    `CoInitializeEx(COINIT_MULTITHREADED)`（wintoastlib.cpp:544）。
		//    因此一旦用 `SHORTCUT_POLICY_IGNORE`，线程上就没有 COM，
		//    `RoGetActivationFactory` 会失败——而 `showToast` 在那个分支上
		//    **不调用 setError**，表现为"返回 -1 但 error 仍是 NoError"。
		//    （这一条已由实测确认，而非仅源码推断；见主文档 §2.3 D5 / D9）
		//
		// 套间模式取 **MTA**，与 WinToast 自身的 `createShortcut()` 保持一致：
		// 它用 `Implements<RuntimeClassFlags<ClassicCom>, …>`（无 FTM）注册事件回调，
		// MTA 下这些对象可被任意 MTA 线程直接调用，无需跨套间封送。
		//
		// 不使用 `Engine/OS/Windows/ComInitalizer.hpp`：它在 `!SUCCEEDED(hr)` 时走
		// `DEBUG_VERIFY_REPORT`，Debug 下会直接中断进程——而 `RPC_E_CHANGED_MODE`
		//（线程已是别的套间模式）恰恰是可容忍的常见情形，不该中断。这里按 WinToast 的
		// 同一判据处理：容忍 `RPC_E_CHANGED_MODE`，且此时不占用初始化计数
		if (!m_comInitialized)
		{
			const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
			m_comInitialized = SUCCEEDED(comResult);   // S_FALSE 也算一次占用，需配对 CoUninitialize

			if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
				return std::unexpected(NotificationError::BackendUnavailable);
		}

		if (!WinToast::isCompatible())
			return std::unexpected(NotificationError::SystemNotSupported);

		// WinToast 要求 AppName 与 AUMI 都非空，否则 initialize 直接失败（InvalidParameters）
		if (config.Application.Empty())
			return std::unexpected(NotificationError::InvalidConfig);

		// configureAUMI 的第三段只在非空时才会追加第四段，因此把 Version 放到第三段，
		// 得到 "organization.application.version" 三段式
		const std::wstring aumi = WinToast::configureAUMI(
			ToWide(config.Organization), ToWide(config.Application), ToWide(config.Version));

		// configureAUMI 内部只在超长时打日志，不会失败——这里自己拒绝
		if (aumi.size() > SCHAR_MAX)
			return std::unexpected(NotificationError::InvalidConfig);

		WinToast::instance()->setAppName(ToWide(config.Application));
		WinToast::instance()->setAppUserModelId(aumi);

		// 快捷方式策略：REQUIRE_CREATE 会在开始菜单写入 <Application>.lnk，把 AUMI 注册给外壳。
		//
		// ⚠️ **实测结论（调研文档 §2.3.3）：动作按钮能否渲染，取决于 AUMI 是否经该 .lnk 注册。**
		//    对照组（旧实现，走默认策略、写出 K.lnk）气泡与按钮都正常；本实现此前用 IGNORE
		//    （无 .lnk）时气泡能显示但**没有任何动作按钮**。仅靠 initialize() 间接调用
		//    SetCurrentProcessExplicitAppUserModelID 不足以渲染交互元素。
		//    因此默认值取 false（库不产生副作用），但**需要动作按钮的调用方必须开启本项**
		WinToast::instance()->setShortcutPolicy(config.AllowShellRegistration
			? WinToast::SHORTCUT_POLICY_REQUIRE_CREATE
			: WinToast::SHORTCUT_POLICY_IGNORE);

		// WinToast 的调试输出会写 stdout（与 std::printf 混流，且非 ASCII 会中断输出），
		// 引擎里保持关闭
		WinToastLib::setDebugOutputEnabled(false);

		WinToast::WinToastError toastError = WinToast::WinToastError::NoError;
		if (!WinToast::instance()->initialize(&toastError))
		{
			// 注意：若用 SHORTCUT_POLICY_IGNORE 且调用方未自行初始化 COM，
			// 失败点会落在 RoGetActivationFactory，此时 toastError 可能仍是 NoError
			return std::unexpected(MapError(toastError));
		}

		m_queue = std::make_shared<CompletionQueue>();
		m_initialized = true;
		return {};
	}

	NotificationCapabilities Win32NotificationBox::Capabilities() const noexcept
	{
		using WinToast = WinToastLib::WinToast;

		NotificationCapabilities caps;

		// 刻意不做缓存：这些判定很便宜，而 Linux 后端的能力是运行时可变的，
		// 保持"每次现查"可以让前端的行为在三平台上一致（主文档 §5.3）
		caps.Available = WinToast::isCompatible();

		const bool modern = caps.Available && WinToast::isSupportingModernFeatures();       // 主版本 > 6
		const bool anniversary = caps.Available && WinToast::isWin10AnniversaryOrHigher();  // build >= 14393

		caps.Image = caps.Available;              // ImageAndText## 模板自 Win8 起可用
		caps.Expiration = caps.Available;         // IToastNotification::put_ExpirationTime

		caps.Actions = modern;
		caps.TextReply = modern;                  // addInput 走 <actions>，同样受现代特性门控
		caps.Attribution = modern;
		caps.Sound = modern;
		caps.CustomSound = modern;
		caps.LoopSound = modern;
		caps.Timeout = modern;                    // setDuration 走 duration 属性

		caps.HeroImage = anniversary;
		caps.CircleCrop = anniversary;

		// WinToast 1.3.2 完全没有暴露的能力（主文档 §2.2 WN1–WN5）
		caps.Grouping = false;
		caps.Badge = false;
		caps.Scheduling = false;
		caps.Query = false;
		caps.Replace = false;

		// 回调：三态 dismiss 与 failed 都是 Windows 侧的强项
		caps.DismissCallback = caps.Available;
		caps.FailedCallback = caps.Available;

		return caps;
	}

	std::expected<U64, NotificationError> Win32NotificationBox::Show(const NotificationRequest& request)
	{
		// WinToast::instance() 是 thread_local：换线程会拿到**另一个未初始化实例**，
		// 症状是 showToast 返回 -1 且 error = NotInitialized/NoError（主文档 §2.3 D1）
		PENFRAMEWORK_VERIFY_CORE_PLUGIN_OWNER_THREAD(m_ownerThread,
			"Win32NotificationBox::Show must run on the owner thread",
			"Win32NotificationBox", "Show must run on the owner thread");

		if (!m_initialized)
			return std::unexpected(NotificationError::NotInitialized);

		using WinToast = WinToastLib::WinToast;
		using WinToastTemplate = WinToastLib::WinToastTemplate;

		// ---- Require 等级的执行 ----
		// 只有"功能性"能力（动作、文本回复）带保证等级：这类能力静默消失会让通知变成
		// 死信息，比不显示更糟；而装饰性能力（Hero 图、裁剪、归因文本）静默降级是可接受的。
		// 这正是主文档 §6.5 的"降级按代价分类"的落地
		{
			const NotificationCapabilities caps = Capabilities();

			if (!request.Actions.empty()
				&& request.ActionsGuarantee == NotificationGuarantee::Require
				&& !caps.Actions)
			{
				return std::unexpected(NotificationError::RequireUnsupported);
			}

			if (request.TextReply
				&& request.TextReplyGuarantee == NotificationGuarantee::Require
				&& !caps.TextReply)
			{
				return std::unexpected(NotificationError::RequireUnsupported);
			}
		}

		const bool hasImage = !request.ImagePath.Empty();
		const bool hasHeroImage = !request.HeroImagePath.Empty();
		const bool hasBody = !request.Body.Empty();

		// 是否保留动作按钮 / 文本回复。
		// ⚠️ WinToast 的 addAction 与 addInput **不能共存**：addInputHelper 会无条件再追加
		//    一个 <actions> 节点，而 toast schema 只允许一个（主文档 §2.3 D2）。
		//    两者都请求时只能保留一个：这里保留 Actions，丢弃 TextReply。
		//    若 TextReply 声明为 Require，前端已在 Show() 中提前拒绝（RequireUnsupported）
		const bool keepActions = !request.Actions.empty();
		const bool keepTextReply = request.TextReply && !keepActions;

		// WinToast 的 setImageFieldHelper 取的是模板内**已有**的 <image> 节点，
		// 因此想带图就必须选 ImageAndText## 系列（Text## 模板里没有 image 节点）
		if (hasImage && !IsAcceptableImagePath(request.ImagePath))
			return std::unexpected(NotificationError::InvalidRequest);

		const WinToastTemplate::WinToastTemplateType type = hasImage
			? (hasBody ? WinToastTemplate::ImageAndText02 : WinToastTemplate::ImageAndText01)
			: (hasBody ? WinToastTemplate::Text02 : WinToastTemplate::Text01);

		WinToastTemplate toast(type);

		toast.setFirstLine(ToWide(request.Title));
		if (hasBody)
			toast.setSecondLine(ToWide(request.Body));

		// ---- 图片 ----
		// ⚠️ 曾经在这里"强制 ToastGeneric 绑定"（借 setImagePath(L"", Circle) 让
		//    isToastGeneric() 为真），以期让动作按钮渲染。**那是错的，已撤销**：
		//    setBindToastGenericHelper 只翻 <binding template> 这一个属性，却把
		//    <text id="1"> 这类**旧式子节点**留在原处；ToastGeneric 不接受这种子节点组合，
		//    于是整个负载变成非法，Windows **直接丢弃**——症状是"连通知都不显示了"。
		//    实测（`K.lnk` 存在 + 默认快捷方式策略）时，旧式 binding 反而能正常渲染动作按钮，
		//    见主文档 §2.3.3 的更正。
		if (hasImage)
		{
			// 圆形裁剪需 build >= 14393；低版本 WinToast 内部会自行降级为方图
			toast.setImagePath(ToWide(request.ImagePath),
				request.CircleCrop ? WinToastTemplate::CropHint::Circle : WinToastTemplate::CropHint::Square);
		}

		// Hero 图需 build >= 14393。注意其路径是**原样**写入（不拼 file:///），
		// 与 setImagePath 不同（主文档 §7.7）
		if (hasHeroImage && WinToast::isWin10AnniversaryOrHigher())
			toast.setHeroImagePath(ToWide(request.HeroImagePath), false);

		// ---- 动作 / 文本回复（keepActions / keepTextReply 已在上面判定）----
		auto record = std::make_shared<Record>();
		record->ActionIds.reserve(request.Actions.size());

		if (keepActions)
		{
			// I7：ActionIds 必须在 showToast **之前**填好——回调可能立刻到达，
			// 届时按 actionIndex 取值必须已就位
			for (const NotificationAction& action : request.Actions)
			{
				toast.addAction(ToWide(action.Label));
				record->ActionIds.push_back(action.Id);
			}
		}
		else if (keepTextReply)
		{
			// 占位符与按钮文案由 WinToast 硬编码，无法自定义
			toast.addInput();
		}

		// ---- 归因文本（主版本 > 6）----
		if (!request.Attribution.Empty() && WinToast::isSupportingModernFeatures())
			toast.setAttributionText(ToWide(request.Attribution));

		// ---- 声音 ----
		if (request.Silent)
			toast.setAudioOption(WinToastTemplate::AudioOption::Silent);

		// ---- 打断强度 ----
		// ⚠️ Windows 侧的 Scenario 是"语义场景"（Alarm/Reminder/IncomingCall），
		//    与本层的 Urgency（打断强度）**不是同一条轴**，无法一一映射（主文档 §7.4）。
		//    因此这里只把强度映射到"停留时长"，不伪造 Scenario
		if (request.Urgency == NotificationUrgency::Critical || request.Urgency == NotificationUrgency::High)
			toast.setDuration(WinToastTemplate::Duration::Long);

		// 注：request.TimeoutMs（横幅停留时长）在 Windows 侧没有独立 API，
		//     它由系统的"轻松使用"设置决定；但 Duration 会覆盖其中一部分。
		//     Linux 的 expire_timeout 才有对应语义（主文档 §7.5）

		// ---- 过期（从操作中心移除）----
		if (request.ExpirationMs > 0)
			toast.setExpiration(static_cast<INT64>(request.ExpirationMs));

		// ---- 投递 ----
		// handler 必须 new：WinToast 用 shared_ptr 接管并会 delete（D3）。
		// 即使 showToast 失败，其内部的 shared_ptr 局部变量也会释放它，不会泄漏
		WinToast::WinToastError toastError = WinToast::WinToastError::NoError;
		const INT64 toastId = WinToast::instance()->showToast(
			toast, new Win32ToastHandler(m_queue, record), &toastError);

		if (toastId < 0)
			return std::unexpected(MapError(toastError));

		// 主线程补写 PlatformId（I6）：回调线程不会读它
		record->PlatformId = static_cast<U64>(toastId);
		return record->PlatformId;
	}

	void Win32NotificationBox::Hide(U64 platformId) noexcept
	{
		if (!m_initialized)
			return;

		// hideToast 只对**同一线程实例** _buffer 内存在的 id 有效，否则返回 false
		WinToastLib::WinToast::instance()->hideToast(static_cast<INT64>(platformId));
	}

	void Win32NotificationBox::Clear() noexcept
	{
		if (!m_initialized)
			return;

		// 注意：WinToast::clear() 只隐藏**当前显示中**的通知，
		// 不会清空操作中心历史，也不会触发回调（前端因此要主动了结在途槽位）
		WinToastLib::WinToast::instance()->clear();
	}

	void Win32NotificationBox::DrainCompleted(std::vector<Internal::BackendCompletion>& out)
	{
		if (!m_queue)
			return;

		std::vector<QueuedCompletion> items;

		{
			const std::lock_guard<std::mutex> guard(m_queue->Mutex);
			items.swap(m_queue->Items);
		}

		for (const QueuedCompletion& item : items)
		{
			// PlatformId 由主线程在 Show 中补写；此刻必然已就位（I6）
			if (item.Owner->PlatformId == 0)
				continue;

			Internal::BackendCompletion completion;
			completion.PlatformId = item.Owner->PlatformId;
			completion.Outcome = item.Outcome;
			completion.ActionId = item.ActionId;
			completion.Text = item.Text;

			out.push_back(std::move(completion));
		}
	}
}
