// File /Native/Engine/OS/Windows/SystemInfo.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "Windows.h"
#include "../../String/String.hpp"
#include <optional>

namespace PenEngine
{
	/// @brief 注册表键的 RAII 句柄
	/// @note I1. 持有者负责在用完后 `RegCloseKey`；因此本类只允许移动，拷贝被删除
	///       （旧实现直接把 `RegOpenKeyExW` 的 out 参数写回了入参 `handle`，
	///       既丢掉了调用方传入的根键、打开的键也从没关过 —— 每次都泄漏一个键句柄）。
	///       I2. 只读打开：`KEY_READ` 即可，不需要写权限。
	class RegistryKey
	{
	public:
		RegistryKey() noexcept = default;
		/// @brief 打开 `root` 下的 `subKey`
		/// @return 成功返回键对象；失败返回 `std::nullopt`（不抛异常：键不存在是正常情况）
		[[nodiscard]] static std::optional<RegistryKey> Open(HKEY root, WStringView subKey) noexcept;

		~RegistryKey() noexcept;

		RegistryKey(const RegistryKey&) = delete;
		RegistryKey& operator=(const RegistryKey&) = delete;
		RegistryKey(RegistryKey&& other) noexcept;
		RegistryKey& operator=(RegistryKey&& other) noexcept;

		[[nodiscard]] bool Valid() const noexcept { return m_key != nullptr; }
		[[nodiscard]] HKEY Handle() const noexcept { return m_key; }

	private:
		explicit RegistryKey(HKEY key) noexcept : m_key(key) {}

		HKEY m_key = nullptr;
	};

	/// @brief 读取一个字符串型注册表值
	/// @param key 已打开的键
	/// @param name 值名
	/// @param outValue 成功时写入取值（UTF-8）
	/// @return 读到返回 true；值不存在 / 类型不是字符串 / 调用失败都返回 false
	/// @note I1. 两段式读取：先按 `REG_SZ` / `REG_EXPAND_SZ` 校验类型，再按实际字节数扩缓冲区。
	///       旧实现固定开 1024 并靠 `ERROR_MORE_DATA` 循环，且从不检查类型 ——
	///       若值是 `REG_DWORD` 会把 4 字节整数当宽字符串解释。
	///       I2. 空串是合法取值（返回 true）；"读不到"与"读到空"必须能区分。
	[[nodiscard]] bool QueryRegistryString(HKEY key, WStringView name, String& outValue);

	/// @brief 从注册表读一个字符串值（内部负责开键）
	/// @return 读不到返回 `std::nullopt`
	[[nodiscard]] std::optional<String> QueryRegistryString(HKEY root, WStringView subKey, WStringView name);

	/// @brief 系统版本信息
	/// @note 各字段用途不同，不要互相替代：
	///       - `Kernel` 来自 `RtlGetVersion`，**不受 manifest / 兼容性垫片影响**，是判断版本号该信谁的依据；
	///       - `ProductName` / `DisplayVersion` 来自注册表，是给人看的名字。
	///       ⚠️ 注册表的 `ProductName` 在 Windows 11 上仍可能写着 "Windows 10"（本机即如此：
	///       ProductName=Windows 10 Pro 而 CurrentBuildNumber=26200）。所以
	///       **判断 Windows 10 / 11 一律看 `Kernel.Build`（>= 22000 为 11），不要解析 ProductName**。
	struct SystemVersion
	{
		/// @brief 注册表 `ProductName`，如 "Windows 10 Pro"（可能过时，仅用于展示）
		String ProductName;
		/// @brief 注册表 `DisplayVersion`，如 "25H2"
		String DisplayVersion;
		/// @brief 注册表 `CurrentBuildNumber`（与 `Kernel.Build` 通常相同）
		U32 RegistryBuild = 0;
		/// @brief 注册表 `UBR`（修订号）
		U32 RegistryRevision = 0;

		/// @brief `RtlGetVersion` 结果
		U32 Major = 0;
		U32 Minor = 0;
		U32 Build = 0;
		U32 Revision = 0;
		/// @brief 系统类型字符串，如 "Windows 11 Pro"（`RtlGetVersion` 的 `szCSDVersion`）
		WString BuildLab;

		/// @brief "Windows 10 Pro 25H2 (Build 26200.9457)" 形态的展示串
		[[nodiscard]] String ToString() const;

		/// @brief 按内核版本号判断是否为 Windows 11 及以上
		/// @note >= 22000 为 Windows 11；不要用 ProductName 判断
		[[nodiscard]] bool IsWindows11OrLater() const noexcept { return Build >= 22000; }

		/// @brief 内核版本串，如 "10.0.26200.9457"
		[[nodiscard]] String KernelVersionString() const;
	};

	/// @brief 取系统版本（Windows）
	/// @note 注册表读不到时对应字段留空，`Kernel` 仍来自 `RtlGetVersion`（它不需要注册表）
	[[nodiscard]] SystemVersion QuerySystemVersion();
}
