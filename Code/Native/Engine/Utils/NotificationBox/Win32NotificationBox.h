// File /Native/Engine/Utils/NotificationBox/Win32NotificationBox.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../../Utils/OwnerThread.hpp"
#include "Internal/INotificationBackend.hpp"
#include "NotificationTypes.hpp"
#include <expected>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace PenEngine
{
	class Win32ToastHandler;   // 定义在 .cpp；需要访问本类的私有嵌套类型

	/// @brief Windows 后端：WinRT Toast 通知（经 WinToast 1.3.2）
	///
	/// @note **刻意不包含 `<wintoastlib.h>`**：该头在全局作用域 `using namespace Microsoft::WRL;`
	///       与 `ABI::Windows::...`，会污染所有包含者。旧框架把它放进头文件（§2.5），
	///       本实现改为只在 `.cpp` 中包含，回调处理器也一并私有化
	///
	/// @note **线程纪律**：`WinToast::instance()` 是 **thread_local** 单例
	///       （`wintoastlib.cpp:451`），每线程独立持有 `_isInitialized` / `_buffer` / `_aumi`。
	///       因此本后端的所有方法都必须在初始化它的那个线程调用——前端已用
	///       `NotificationBox::IsOwnerThread()` 拦截，这里再断言一次
	class Win32NotificationBox final : public Internal::INotificationBackend
	{
	public:
		Win32NotificationBox() noexcept;
		virtual ~Win32NotificationBox() noexcept override;

		Win32NotificationBox(const Win32NotificationBox&) = delete;
		Win32NotificationBox& operator=(const Win32NotificationBox&) = delete;

		[[nodiscard]] StringView Name() const noexcept override;

		std::expected<void, NotificationError> Initialize(const NotificationBoxConfig& config) override;
		[[nodiscard]] NotificationCapabilities Capabilities() const noexcept override;

		std::expected<U64, NotificationError> Show(const NotificationRequest& request) override;
		void Hide(U64 platformId) noexcept override;
		void Clear() noexcept override;

		void DrainCompleted(std::vector<Internal::BackendCompletion>& out) override;

	private:
		/// @brief 单条通知的共享状态——后端与 WinToast 回调之间的桥梁
		/// @note 关键点：WinToast 的 `showToast` **先接收 handler、后返回 id**，
		///       所以 handler 无法在构造时知道自己属于哪条通知。
		///       解法是让 handler 只携带本状态指针，`PlatformId` 由后端在
		///       `showToast` 返回后补写；回调线程**不读** `PlatformId`，
		///       只有主线程在 drain 时才读，因此无需加锁（见下方 I6）
		struct Record
		{
			/// 动作 Id 表：WinToast 回传**下标**，据此还原成调用方的稳定 Id
			std::vector<String> ActionIds;

			/// 平台侧 id（WinToast 的 `GUID.Data1` 截断值）。主线程写、主线程读
			U64 PlatformId = 0;
		};

		/// @brief 回调线程投递的一项（尚未解析 PlatformId）
		struct QueuedCompletion
		{
			std::shared_ptr<Record> Owner;
			NotificationOutcome Outcome = NotificationOutcome::None;
			String ActionId;
			String Text;
		};

		/// @brief 回调线程与主线程共享的完成队列
		/// @note WinToast 的事件用 `Implements<RuntimeClassFlags<ClassicCom>, …>` 注册且无 FTM，
		///       回调**可能在非主线程**触发，且 `ObjectSignal` 无锁——所以这里只入队，
		///       绝不直接 Emit（前端 §6.6 / `I2`）
		struct CompletionQueue
		{
			std::mutex Mutex;
			std::vector<QueuedCompletion> Items;
		};

		friend class Win32ToastHandler;

		static bool IsAcceptableImagePath(StringView path) noexcept;

		std::shared_ptr<CompletionQueue> m_queue;
		OwnerThread m_ownerThread;

		/// @brief 本后端是否持有一次待配对的 COM 初始化
		/// @note 只有 `SUCCEEDED(CoInitializeEx(...))`（含 `S_FALSE`）才置位。
		///       `RPC_E_CHANGED_MODE` 说明该线程已是**别的套间模式**，此时我们并未增加
		///       初始化计数，析构也就**不得**调用 `CoUninitialize`
		bool m_comInitialized = false;
		bool m_initialized = false;
	};

	// ================================================================
	// 补充不变量（承接 NotificationBox 的 I1–I5）
	//
	//   I6 Record::PlatformId 由主线程在 `showToast` 返回后写、由主线程在 drain 时读；
	//      回调线程只读 ActionIds（构造后只读）与写队列，**不读 PlatformId**。
	//      因此该字段无需加锁，队列本身由 CompletionQueue::Mutex 保护
	//   I7 ActionIds 必须在 `showToast` 之前填充完毕：回调可能在投递后立刻到达，
	//      届时按 actionIndex 取值必须已经就位
	// ================================================================
}
