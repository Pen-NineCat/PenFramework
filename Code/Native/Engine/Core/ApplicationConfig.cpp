// File /Native/Engine/Core/ApplicationConfig.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "ApplicationConfig.h"

#include "../Exception/Exception.hpp"
#include "../IO/Filesystem/FileDevice.hpp"
#include "../Utils/CommandLineParser.hpp"

namespace PenEngine
{
	// ================================================================
	// 配置项清单：解析器与配置查找共用同一份
	// ================================================================
	std::span<const ConfigOption> ApplicationConfigOptions()
	{
		static const ConfigOption Options[] =
		{
			{ "application.title",      "窗口标题",                       true },
			{ "application.version",    "应用版本字符串（仅用于日志与关于界面）", true },
			{ "window.width",           "窗口客户区宽度（像素）",           true },
			{ "window.height",          "窗口客户区高度（像素）",           true },
			{ "window.maximized",       "启动时最大化",                    true },
			{ "timing.max_tps",         "固定更新步长的目标频率",           true },
			{ "timing.max_fps",         "渲染帧率上限",                    true },
			{ "render.interface",       "渲染后端（当前尚未接入 RHI，仅记录）", true },
			{ "render.enable_rhi_thread",    "启用 RHI 线程",              true },
			{ "render.enable_render_thread", "启用渲染线程",               true },
		};

		return Options;
	}

	// ================================================================
	// 小工具
	// ================================================================
	namespace Internal
	{
		StringView TrimConfigText(StringView text) noexcept
		{
			Usize begin = 0;
			Usize end = text.Size();

			while (begin < end)
			{
				const char ch = text.Data()[begin];
				if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
					break;
				++begin;
			}

			while (end > begin)
			{
				const char ch = text.Data()[end - 1];
				if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
					break;
				--end;
			}

			return text.Subview(begin, end - begin);
		}

		bool TryParseConfigBool(StringView text, bool& result) noexcept
		{
			auto equalsIgnoreCase = [&text](StringView candidate) noexcept
			{
				if (text.Size() != candidate.Size())
					return false;
				for (Usize i = 0; i < text.Size(); ++i)
				{
					char lhs = text.Data()[i];
					char rhs = candidate.Data()[i];
					if (lhs >= 'A' && lhs <= 'Z')
						lhs = static_cast<char>(lhs - 'A' + 'a');
					if (rhs >= 'A' && rhs <= 'Z')
						rhs = static_cast<char>(rhs - 'A' + 'a');
					if (lhs != rhs)
						return false;
				}
				return true;
			};

			if (equalsIgnoreCase("true") || equalsIgnoreCase("1") || equalsIgnoreCase("yes") || equalsIgnoreCase("on"))
			{
				result = true;
				return true;
			}

			if (equalsIgnoreCase("false") || equalsIgnoreCase("0") || equalsIgnoreCase("no") || equalsIgnoreCase("off"))
			{
				result = false;
				return true;
			}

			return false;
		}
	}

	std::optional<String> QueryJsonPath(const Json& root, StringView dottedPath)
	{
		if (dottedPath.Empty())
			return std::nullopt;

		// 逐段下降：每一段都必须在当前层里存在、且当前层是对象
		const Json* current = &root;
		Usize begin = 0;

		while (begin <= dottedPath.Size())
		{
			Usize dot = begin;
			while (dot < dottedPath.Size() && dottedPath.Data()[dot] != '.')
				++dot;

			const StringView segment(dottedPath.Data() + begin, dot - begin);
			if (segment.Empty())
				return std::nullopt;

			const auto* object = current->TryGetValueAs<Internal::JsonObject>();
			if (object == nullptr)
				return std::nullopt;

			const auto entry = object->find(String(segment));
			if (entry == object->end())
				return std::nullopt;

			current = &entry->second;

			if (dot >= dottedPath.Size())
				break;

			begin = dot + 1;
		}

		// 按值返回：调用方拿到的是自有副本，不依赖任何内部缓冲区的生命周期
		if (const auto* text = current->TryGetValueAs<String>(); text != nullptr)
			return *text;

		if (const auto* boolean = current->TryGetValueAs<bool>(); boolean != nullptr)
			return String(*boolean ? "true" : "false");

		if (const auto* signedValue = current->TryGetValueAs<I64>(); signedValue != nullptr)
		{
			String number;
			number.ConvertAndPushBack(*signedValue);
			return number;
		}

		if (const auto* unsignedValue = current->TryGetValueAs<U64>(); unsignedValue != nullptr)
		{
			String number;
			number.ConvertAndPushBack(*unsignedValue);
			return number;
		}

		if (const auto* floatingValue = current->TryGetValueAs<double>(); floatingValue != nullptr)
		{
			String number;
			number.ConvertAndPushBack(*floatingValue);
			return number;
		}

		// 对象与数组不构成一个可解析的取值
		return std::nullopt;
	}

	// ================================================================
	// ApplicationConfig
	// ================================================================
	Path ApplicationConfig::GetDefaultConfigPath()
	{
		return Path::GetApplicationPath() / "ApplicationData" / "ApplicationConfiguration.json";
	}

	bool ApplicationConfig::LoadFile(const Path& path)
	{
		const Path target = path.Empty() ? GetDefaultConfigPath() : path;
		m_configFilePath = target.ToString<char>();
		m_fileLoaded = false;

		// I4：文件不存在不是错误 —— 配置层直接缺席，由默认值兜底
		if (!FileDevice::IsRegularFile(target))
			return false;

		FileDevice device;
		if (device.Open(target, FileDevice::Mode::CanRead | FileDevice::Mode::Text) != FileDevice::OperationResult::Success)
			return false;

		String content;
		const auto readResult = device.ReadAllTo(content);
		device.Close();

		if (!readResult.has_value())
			return false;

		Json parsed;
		if (!parsed.Parse(content).has_value())
			return false;

		// 顶层必须是对象，否则整份配置没有可查的键
		if (parsed.TryGetValueAs<Internal::JsonObject>() == nullptr)
			return false;

		m_file = std::move(parsed);
		m_fileLoaded = true;
		return true;
	}

	void ApplicationConfig::LoadCommandLine(const CommandLineParseResult& options, std::vector<String> argumentVector)
	{
		m_argumentVector = std::move(argumentVector);

		// I5：把命令行层拷成自有表，之后不再引用解析结果。
		// 只遍历已声明的配置项 —— 其他选项（如内置 --help）与配置无关，不必进表
		m_commandLineValues.clear();
		for (const ConfigOption& option : ApplicationConfigOptions())
		{
			if (!options.Has(option.Key))
				continue;

			if (auto value = options.Get<String>(option.Key); value.has_value())
				m_commandLineValues.insert_or_assign(String(option.Key), std::move(*value));
		}
	}

	void ApplicationConfig::AddWarning(StringView key, ConfigLayer layer, StringView text, StringView expected) const
	{
		String warning;
		warning += key;
		warning += "：";
		warning += ConfigLayerName(layer);
		warning += "层取值 '";
		warning += text;
		warning += "' 不是合法的";
		warning += expected;
		m_warnings.push_back(std::move(warning));
	}

	std::vector<ConfigCandidate> ApplicationConfig::Lookup(StringView key) const
	{
		std::vector<ConfigCandidate> candidates;

		// 第一层：命令行（自有表，见 I5）。键名与声明时一致，因此直接同名查找
		if (const auto entry = m_commandLineValues.find(key); entry != m_commandLineValues.end())
			candidates.push_back(ConfigCandidate{ entry->second, ConfigLayer::CommandLine });

		// 第二层：配置文件
		if (m_fileLoaded)
		{
			if (auto value = QueryJsonPath(m_file, key); value.has_value())
				candidates.push_back(ConfigCandidate{ std::move(*value), ConfigLayer::File });
		}

		return candidates;
	}

	String ApplicationConfig::GetString(StringView key, StringView defaultValue) const
	{
		if (auto resolved = Resolve<String>(key); resolved.has_value())
			return std::move(resolved->first);
		return String(defaultValue);
	}

	bool ApplicationConfig::GetBool(StringView key, bool defaultValue) const
	{
		if (auto resolved = Resolve<bool>(key); resolved.has_value())
			return resolved->first;
		return defaultValue;
	}

	U32 ApplicationConfig::GetUInt(StringView key, U32 defaultValue) const
	{
		if (auto resolved = Resolve<U32>(key); resolved.has_value())
			return resolved->first;
		return defaultValue;
	}

	I32 ApplicationConfig::GetInt(StringView key, I32 defaultValue) const
	{
		if (auto resolved = Resolve<I32>(key); resolved.has_value())
			return resolved->first;
		return defaultValue;
	}

	float ApplicationConfig::GetFloat(StringView key, float defaultValue) const
	{
		if (auto resolved = Resolve<float>(key); resolved.has_value())
			return resolved->first;
		return defaultValue;
	}
}
