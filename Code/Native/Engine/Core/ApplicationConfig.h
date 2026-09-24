// File /Native/Engine/Core/ApplicationConfig.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "ConfigSource.h"
#include "../IO/Path.h"
#include "../Json/Json.h"
#include "../String/StringUnorderedMap.hpp"
#include <charconv>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace PenEngine
{
	class CommandLineParseResult;

	/// @brief 某一层为某个键提供的候选取值
	struct ConfigCandidate
	{
		/// @brief 该层的取值原文（自有副本）
		String Text;
		/// @brief 来源层
		ConfigLayer Layer = ConfigLayer::Default;
	};

	/// @brief 一个可由「命令行 / 配置文件 / 默认值」任一层提供的配置项
	/// @note I1. `Key` 同时是配置文件里的点分路径与命令行长名：
	///       `"window.width"` ↔ JSON 的 `{ "window": { "width": ... } }` ↔ `--window.width`。
	///       三层共用同一个名字，不需要任何映射表，也就不存在三处名字漂移的可能。
	///       I2. `TakesValue == false` 的项是开关：出现即真，只能由命令行提供。
	struct ConfigOption
	{
		/// @brief 配置键，点分小写 snake_case（同时是 CLI 长名）
		StringView Key;
		/// @brief `-h` / `--help` 中的说明
		StringView Help;

		/// @brief true = 需要取值（CLI 可给 `--key value` 或 `--key=value`）
		bool TakesValue = true;
	};

	/// @brief 本引擎认识的配置项清单
	/// @return 静态表；解析器与配置查找共用同一份，避免两处各写一遍
	[[nodiscard]] std::span<const ConfigOption> ApplicationConfigOptions();

	/// @brief `"window.width"` → `{"window": {"width": ...}}` 的路径查找
	/// @return 命中返回**自有副本**（数值会被转成十进制文本）；任一层缺失或类型不是标量时返回 `std::nullopt`
	/// @note 只读、不抛异常：配置文件里少一个键是正常情况，由下一层兜底
	[[nodiscard]] std::optional<String> QueryJsonPath(const Json& root, StringView dottedPath);

	/// @brief 解析字符串到 `T`
	/// @return 成功返回值；失败返回 `std::nullopt`（调用方继续往下一层找）
	/// @note 支持 `String` / `bool` 与全部算术类型，被 `Get*` 系列复用
	template <typename T>
	[[nodiscard]] std::optional<T> ParseConfigValue(StringView text);

	/// @brief 类型的中文名，用于「取值非法」的警告文案
	template <typename T>
	[[nodiscard]] constexpr StringView ConfigTypeName() noexcept
	{
		using ValueType = std::remove_cv_t<T>;

		if constexpr (std::is_same_v<ValueType, bool>)
			return "布尔值（true/false）";
		else if constexpr (std::is_same_v<ValueType, String>)
			return "文本";
		else if constexpr (std::is_floating_point_v<ValueType>)
			return "浮点数";
		else if constexpr (std::is_unsigned_v<ValueType>)
			return "非负整数";
		else if constexpr (std::is_integral_v<ValueType>)
			return "整数";
		else
			return "值";
	}
	/// @brief 应用启动配置：按「命令行 → 配置文件 → 默认值」三层取值
	/// @note I1. 优先级严格按上述顺序；同一项由更高优先层命中后，低层不再参与。
	///       I2. 任一层取值**解析失败**（例如命令行给了 `--window.width=abc`）不会中止启动，
	///       而是像该层没提供一样继续往下降，并把原因写进 ParseWarnings()。
	///       I3. 本对象只在启动期构造与读取，运行期不再变化。
	///       I4. 配置文件缺失或 JSON 非法都不是致命错误：前者直接跳过该层，
	///       后者经 `ConfigFilePath()` / `ParseWarnings()` 可查。
	///       I5. **本对象自有全部数据**，不引用命令行解析结果：命令行层在
	///       `LoadCommandLine` 时被拷贝成 `键 → 值` 表，原始参数一并留档。
	///       因此不存在"解析结果必须活得比配置久"这类生命周期约束。
	class ApplicationConfig
	{
	public:
		ApplicationConfig() = default;

		/// @brief 从磁盘加载配置文件
		/// @param path 配置文件路径；为空时用默认位置（可执行文件目录 / ApplicationData / ...）
		/// @return 成功解析返回 true；文件不存在或 JSON 非法返回 false（此时该层不参与取值）
		bool LoadFile(const Path& path = {});

		/// @brief 装载命令行层（见 I5：取值被拷贝，不保留引用）
		/// @param options 已解析完成的命令行结果
		/// @param argumentVector 原始参数（含 `argv[0]`）
		void LoadCommandLine(const CommandLineParseResult& options, std::vector<String> argumentVector);

		// ---- 三层取值（默认值由调用点给出，因此「默认」永远是显式可见的） ----
		// I2 的实现方式：每个 Get* 都走 Resolve<T>，它逐层「取值 + 解析」，
		// 第一个解析成功的层才算命中。因此下面的 GetLayer<T> 与取值结果必然一致。

		[[nodiscard]] String GetString(StringView key, StringView defaultValue) const;
		[[nodiscard]] bool GetBool(StringView key, bool defaultValue) const;
		[[nodiscard]] U32 GetUInt(StringView key, U32 defaultValue) const;
		[[nodiscard]] I32 GetInt(StringView key, I32 defaultValue) const;
		[[nodiscard]] float GetFloat(StringView key, float defaultValue) const;

		/// @brief 逐层查找并解析，返回生效值与其来源层
		/// @return `std::nullopt` 表示三层都没有可解析的取值，调用方应使用自己的默认值
		/// @note `T` 必须是 `String` 或算术/布尔类型（见 `ParseConfigValue`）
		template <typename T>
		[[nodiscard]] std::optional<std::pair<T, ConfigLayer>> Resolve(StringView key) const;

		/// @brief 某项实际由哪一层提供
		/// @return 命中层的枚举；`std::nullopt` 表示三层都没有可解析的取值（用了调用方的默认值）
		/// @note 必须用与取值**相同**的 `T` 调用，否则可能给出与实际取值不同的层
		///       （例如命令行给了 `--window.width=abc` 时，`GetUInt` 会跳过命令行层）
		template <typename T>
		[[nodiscard]] std::optional<ConfigLayer> GetLayer(StringView key) const;

		/// @brief 配置文件路径（`LoadFile` 无论成功与否都会记录，便于报错）
		[[nodiscard]] StringView ConfigFilePath() const noexcept { return m_configFilePath; }
		/// @brief 配置文件是否成功加载
		[[nodiscard]] bool FileLoaded() const noexcept { return m_fileLoaded; }
		/// @brief 各层提供但解析失败的项，形如 `"window.width: 命令行值 'abc' 无法解析"`（诊断用）
		[[nodiscard]] const std::vector<String>& ParseWarnings() const noexcept { return m_warnings; }
		/// @brief 原始命令行（含 `argv[0]`），诊断用
		[[nodiscard]] const std::vector<String>& ArgumentVector() const noexcept { return m_argumentVector; }

		/// @brief 默认配置文件相对可执行文件目录的位置
		[[nodiscard]] static Path GetDefaultConfigPath();

	private:
		/// @brief 收集各层为该键提供的候选，**按优先级从高到低**
		/// @return 命中的候选列表；三层都没有时为空
		/// @note 返回列表而非单个值：高优先层可能提供了值却无法解析成目标类型，
		///       此时必须继续往下降级，所以不能只看第一个
		[[nodiscard]] std::vector<ConfigCandidate> Lookup(StringView key) const;

		/// @brief 记录一条解析失败原因
		void AddWarning(StringView key, ConfigLayer layer, StringView text, StringView expected) const;

		/// @brief 配置文件内容；未加载成功时为空对象
		Json m_file;
		/// @brief 命令行层：配置键 → 取值原文（见 I5，自有副本）
		StringUnorderedMap<String> m_commandLineValues;

		std::vector<String> m_argumentVector;
		String m_configFilePath;
		bool m_fileLoaded = false;
		/// @note `mutable`：取值是 const 操作，但需要记录诊断信息
		mutable std::vector<String> m_warnings;
	};

	// ================================================================
	// 取值转换：模板定义放在头文件末尾，定义处依赖内部辅助函数
	// ================================================================

	namespace Internal
	{
		/// @brief 去掉首尾空白
		[[nodiscard]] StringView TrimConfigText(StringView text) noexcept;

		/// @brief `"true"` / `"1"` / `"yes"` / `"on"`（不区分大小写）→ true，其余 → false
		/// @return 是否是可识别的布尔字面量
		[[nodiscard]] bool TryParseConfigBool(StringView text, bool& result) noexcept;
	}

	template <typename T>
	std::optional<T> ParseConfigValue(StringView text)
	{
		using ValueType = std::remove_cv_t<T>;

		const StringView trimmed = Internal::TrimConfigText(text);

		if constexpr (std::is_same_v<ValueType, String>)
		{
			return String(trimmed);
		}
		else if constexpr (std::is_same_v<ValueType, bool>)
		{
			bool value = false;
			if (!Internal::TryParseConfigBool(trimmed, value))
				return std::nullopt;
			return value;
		}
		else if constexpr (std::is_floating_point_v<ValueType>)
		{
			ValueType value{};
			const char* begin = trimmed.Data();
			const char* end = begin + trimmed.Size();
			const auto [position, code] = std::from_chars(begin, end, value);
			if (code != std::errc() || position != end)
				return std::nullopt;
			return value;
		}
		else if constexpr (std::is_unsigned_v<ValueType>)
		{
			// 与命令行解析一致：from_chars 对无符号类型会把 "-1" 静默回绕，因此先按有符号解析
			I64 signedValue = 0;
			const char* begin = trimmed.Data();
			const char* end = begin + trimmed.Size();
			const auto [position, code] = std::from_chars(begin, end, signedValue);
			if (code != std::errc() || position != end || signedValue < 0)
				return std::nullopt;
			if (static_cast<U64>(signedValue) > static_cast<U64>(std::numeric_limits<ValueType>::max()))
				return std::nullopt;
			return static_cast<ValueType>(signedValue);
		}
		else if constexpr (std::is_integral_v<ValueType>)
		{
			ValueType value{};
			const char* begin = trimmed.Data();
			const char* end = begin + trimmed.Size();
			const auto [position, code] = std::from_chars(begin, end, value);
			if (code != std::errc() || position != end)
				return std::nullopt;
			return value;
		}
		else
		{
			static_assert(!sizeof(ValueType), "ParseConfigValue<T> 只支持 String / bool / 算术类型");
		}
	}

	// Resolve / GetLayer 的模板定义：依赖 ParseConfigValue 与 Lookup，故置于文件末尾
	template <typename T>
	std::optional<std::pair<T, ConfigLayer>> ApplicationConfig::Resolve(StringView key) const
	{
		// 按优先级逐层「取值 + 解析」：第一个解析成功的层才算命中。
		// 高优先层提供了解析不了的取值时，只记一条警告就继续往下降级 —— 这就是 I2。
		for (const ConfigCandidate& candidate : Lookup(key))
		{
			if (auto parsed = ParseConfigValue<T>(candidate.Text); parsed.has_value())
				return std::make_pair(std::move(*parsed), candidate.Layer);

			AddWarning(key, candidate.Layer, candidate.Text, ConfigTypeName<T>());
		}

		return std::nullopt;
	}

	template <typename T>
	std::optional<ConfigLayer> ApplicationConfig::GetLayer(StringView key) const
	{
		if (auto resolved = Resolve<T>(key); resolved.has_value())
			return resolved->second;
		return std::nullopt;
	}
}
