// File /Native/Engine/OS/Windows/SystemInfo.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "SystemInfo.h"

#include <charconv>
#include <format>
#include <utility>
#include <vector>

namespace PenEngine
{
	namespace
	{
		/// @brief 系统版本注册表位置（`ProductName` / `DisplayVersion` / `CurrentBuildNumber` / `UBR`）
		constexpr WStringView VersionRegistryPath = LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)";

		/// @brief `RtlGetVersion` 的入参/出参结构
		/// @note SDK 的 `winternl.h` 不带它（那是 WDK 的 `ntddk.h` 内容），
		///       而它又是**唯一不受 manifest / 兼容性垫片影响**的版本来源，
		///       因此这里按公开文档的 `RTL_OSVERSIONINFOW` 布局自带声明。
		///       布局与 `OSVERSIONINFOW` 前缀一致，字段顺序不可改。
		struct RtlOsVersionInfoW
		{
			ULONG dwOSVersionInfoSize;
			ULONG dwMajorVersion;
			ULONG dwMinorVersion;
			ULONG dwBuildNumber;
			ULONG dwPlatformId;
			WCHAR szCSDVersion[128];
		};

		/// @brief 读一个注册表 DWORD（`REG_DWORD`；读不到或类型不符返回 0）
		[[nodiscard]] U32 QueryRegistryDword(HKEY key, WStringView name) noexcept
		{
			DWORD type = 0;
			DWORD value = 0;
			DWORD size = sizeof(value);

			const LSTATUS status = RegQueryValueExW(key, name.Data(), nullptr, &type,
				reinterpret_cast<LPBYTE>(&value), &size);
			if (status != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(value))
				return 0;

			return static_cast<U32>(value);
		}

		/// @brief 读一个注册表数值，**`REG_DWORD` 与字符串两种形态都接受**
		/// @return 读不到返回 0
		/// @note `CurrentBuildNumber` 在本机是 `REG_SZ`（"26200"）而不是 DWORD ——
		///       按类型硬读会静默拿到 0，所以两种都要认
		[[nodiscard]] U32 QueryRegistryNumeric(HKEY key, WStringView name)
		{
			if (const U32 dword = QueryRegistryDword(key, name); dword != 0)
				return dword;

			String text;
			if (!QueryRegistryString(key, name, text) || text.Empty())
				return 0;

			U32 value = 0;
			const char* begin = text.Data();
			const char* end = begin + text.Size();
			const auto [position, code] = std::from_chars(begin, end, value);
			if (code != std::errc())
				return 0;

			return value;
		}

		/// @brief 取 `RtlGetVersion` 结果；不可用时返回全 0
		[[nodiscard]] RtlOsVersionInfoW QueryKernelVersion() noexcept
		{
			RtlOsVersionInfoW info{};
			info.dwOSVersionInfoSize = sizeof(RtlOsVersionInfoW);

			// ntdll 的导出：签名等价于 LONG NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW)
			using RtlGetVersionFn = LONG(WINAPI*)(RtlOsVersionInfoW*);

			const HMODULE module = GetModuleHandleW(L"ntdll.dll");
			if (module == nullptr)
				return info;

			const auto function = reinterpret_cast<RtlGetVersionFn>(
				GetProcAddress(module, "RtlGetVersion"));
			if (function == nullptr)
				return info;

			if (function(&info) < 0)
				return RtlOsVersionInfoW{};

			return info;
		}
	}

	// ================================================================
	// RegistryKey
	// ================================================================
	std::optional<RegistryKey> RegistryKey::Open(HKEY root, WStringView subKey) noexcept
	{
		if (root == nullptr || subKey.Empty())
			return std::nullopt;

		HKEY opened = nullptr;
		// I1：out 参数必须是**局部变量**。旧实现写成 `RegOpenKeyExW(handle, path, 0, KEY_READ, &handle)`，
		// 把根键覆盖成了子键句柄 —— 之后既无法再引用根键，打开的键也再没人关
		const LSTATUS status = RegOpenKeyExW(root, subKey.Data(), 0, KEY_READ, &opened);
		if (status != ERROR_SUCCESS)
			return std::nullopt;

		return RegistryKey(opened);
	}

	RegistryKey::~RegistryKey() noexcept
	{
		if (m_key != nullptr)
			RegCloseKey(m_key);
	}

	RegistryKey::RegistryKey(RegistryKey&& other) noexcept : m_key(std::exchange(other.m_key, nullptr))
	{}

	RegistryKey& RegistryKey::operator=(RegistryKey&& other) noexcept
	{
		if (this == &other)
			return *this;

		if (m_key != nullptr)
			RegCloseKey(m_key);

		m_key = std::exchange(other.m_key, nullptr);
		return *this;
	}

	// ================================================================
	// 注册表取值
	// ================================================================
	bool QueryRegistryString(HKEY key, WStringView name, String& outValue)
	{
		if (key == nullptr || name.Empty())
			return false;

		DWORD type = 0;
		DWORD byteSize = 0;

		// 第一遍：只问类型与长度。I1：类型必须先校验，否则 REG_DWORD 会被当成字符串解释
		LSTATUS status = RegQueryValueExW(key, name.Data(), nullptr, &type, nullptr, &byteSize);
		if (status != ERROR_SUCCESS)
			return false;

		if (type != REG_SZ && type != REG_EXPAND_SZ)
			return false;

		// 空串（含只有一个结尾符）也是合法取值
		if (byteSize == 0)
		{
			outValue.Clear();
			return true;
		}

		if (byteSize % sizeof(wchar_t) != 0)
			return false;

		// 第二遍：按实际长度取内容。多留一个宽字符余量，避免结尾符写不进去
		const Usize characterCapacity = byteSize / sizeof(wchar_t) + 1;
		std::vector<wchar_t> buffer(characterCapacity, L'\0');

		DWORD capacityBytes = static_cast<DWORD>(buffer.size() * sizeof(wchar_t));
		status = RegQueryValueExW(key, name.Data(), nullptr, nullptr,
			reinterpret_cast<LPBYTE>(buffer.data()), &capacityBytes);
		if (status != ERROR_SUCCESS)
			return false;

		// 值里可能自带结尾符；也可能没有（此时按实际字节数截断）
		Usize characterCount = capacityBytes / sizeof(wchar_t);
		if (characterCount > 0 && buffer[characterCount - 1] == L'\0')
			--characterCount;

		WString wide;
		wide.ConvertFrom(buffer.data(), characterCount);
		outValue.ConvertFrom(wide);
		return true;
	}

	std::optional<String> QueryRegistryString(HKEY root, WStringView subKey, WStringView name)
	{
		auto key = RegistryKey::Open(root, subKey);
		if (!key.has_value())
			return std::nullopt;

		String value;
		if (!QueryRegistryString(key->Handle(), name, value))
			return std::nullopt;

		return value;
	}

	// ================================================================
	// 系统版本
	// ================================================================
	String SystemVersion::KernelVersionString() const
	{
		return std::format("{}.{}.{}.{}", Major, Minor, Build, Revision);
	}

	String SystemVersion::ToString() const
	{
		String result;

		// 人类可读名优先用注册表的 ProductName
		if (!ProductName.Empty())
			result += ProductName;
		else if (Build >= 22000)
			result += "Windows 11";
		else if (Build >= 10240)
			result += "Windows 10";
		else
			result += "Windows";

		if (!DisplayVersion.Empty())
		{
			result += " ";
			result += DisplayVersion;
		}

		// 版本号一律以 RtlGetVersion 为准，注册表 UBR 只作为补充
		result += " (Build ";
		result += std::format("{}.{}", Build, Revision != 0 ? Revision : RegistryRevision);
		result += ")";

		return result;
	}

	SystemVersion QuerySystemVersion()
	{
		SystemVersion version;

		// 注册表：给人看的名字（本机实测 ProductName 可能仍是 "Windows 10 Pro"，见头文件说明）
		if (auto key = RegistryKey::Open(HKEY_LOCAL_MACHINE, VersionRegistryPath); key.has_value())
		{
			String value;

			if (QueryRegistryString(key->Handle(), L"ProductName", value))
				version.ProductName = std::move(value);

			if (QueryRegistryString(key->Handle(), L"DisplayVersion", value))
				version.DisplayVersion = std::move(value);

			version.RegistryBuild = QueryRegistryNumeric(key->Handle(), L"CurrentBuildNumber");
			if (version.RegistryBuild == 0)
				version.RegistryBuild = QueryRegistryNumeric(key->Handle(), L"CurrentBuild");

			version.RegistryRevision = QueryRegistryNumeric(key->Handle(), L"UBR");
			if (version.RegistryRevision == 0)
				version.RegistryRevision = QueryRegistryNumeric(key->Handle(), L"BaseBuildRevisionNumber");
		}

		// 内核版本：权威来源，不依赖注册表
		const RtlOsVersionInfoW kernel = QueryKernelVersion();
		version.Major = kernel.dwMajorVersion;
		version.Minor = kernel.dwMinorVersion;
		version.Build = kernel.dwBuildNumber;
		version.BuildLab = kernel.szCSDVersion;

		// 注册表缺失时至少保证 Build 可用。
		// 修订号（UBR）只有注册表有：RTL_OSVERSIONINFOW 本身不含它
		if (version.Build == 0)
			version.Build = version.RegistryBuild;

		version.Revision = version.RegistryRevision;

		return version;
	}
}
