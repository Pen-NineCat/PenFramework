// File /Native/Engine/Utils/Logger/Logger.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// 本文件是**唯一**允许包含 spdlog 头的翻译单元（门面 Logger.hpp 不出现任何 spdlog 类型）。
//
// 编译期开关由根 CMakeLists.txt 以目标级定义给出（绝不能写在头文件里）：
//   SPDLOG_USE_STD_FORMAT  让 spdlog 用 std::format 而非自带的 fmt —— 与仓库既有的
//                          std::formatter 特化一致，也不引入第二套格式化库。

#include "Logger.hpp"

#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef PENFRAMEWORK_OS_WIN32
	// OutputDebugString：Native（无控制台）形态下唯一可见的出口
	#include <spdlog/sinks/msvc_sink.h>
#endif

#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace PenEngine
{
	namespace
	{
		/// @brief 门面等级 → 后端等级
		[[nodiscard]] constexpr spdlog::level::level_enum ToBackendLevel(LoggerLevel level) noexcept
		{
			switch (level)
			{
			case LoggerLevel::Trace:    return spdlog::level::trace;
			case LoggerLevel::Debug:    return spdlog::level::debug;
			case LoggerLevel::Info:     return spdlog::level::info;
			case LoggerLevel::Warn:     return spdlog::level::warn;
			case LoggerLevel::Error:    return spdlog::level::err;
			case LoggerLevel::Critical: return spdlog::level::critical;
			case LoggerLevel::Off:      break;
			}
			return spdlog::level::off;
		}

		/// @brief 无后端时的兜底输出：至少让 Warn 以上可见（不复制旧件"静默空指针"的行为）
		void FallbackOutput(LoggerLevel level, std::string_view message) noexcept
		{
			if (level < LoggerLevel::Warn)
				return;

			std::fwrite(message.data(), 1, message.size(), stderr);
			std::fputc('\n', stderr);
		}
	}

	/// @brief 后端实现：spdlog 的 logger 与 sink 只在这里出现
	struct Logger::Impl
	{
		std::unique_ptr<spdlog::logger> Instance;

		/// @brief 门面自己的阈值副本；未初始化时为 Warn（与 ShouldLog 一致）
		LoggerLevel Level = LoggerLevel::Warn;

		/// @brief Error / Critical 是否自动附栈（配置副本）
		bool CaptureStacktraceOnError = true;
	};

	namespace Internal
	{
		std::string& LogScratchBuffer()
		{
			static thread_local std::string buffer;
			buffer.reserve(512);
			return buffer;
		}
	}

	Logger::Logger() noexcept = default;

	Logger::~Logger() noexcept
	{
		Shutdown();
	}

	std::expected<void, LoggerError> Logger::Initialize(const LoggerConfig& config)
	{
		// I2：先校验配置，再碰后端；任何失败都不留下半初始化状态
		if (m_impl)
			return std::unexpected(LoggerError::AlreadyInitialized);

		if (config.FilePath.Empty() || config.MaxFileSize == 0 || config.MaxFileCount == 0)
			return std::unexpected(LoggerError::InvalidConfiguration);

		try
		{
			std::vector<spdlog::sink_ptr> sinks;
			sinks.reserve(2);

			// 日志目录不存在时由文件后端自行创建（spdlog 的 file_helper::open 会 create_dir）
			sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
				std::string(config.FilePath.Data(), config.FilePath.Size()),
				config.MaxFileSize,
				config.MaxFileCount));

		#ifdef PENFRAMEWORK_OS_WIN32
			if (config.EnableConsoleSink)
				sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
			else
				sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
		#else
			// 旧件在 Windows 之外根本编不过（调研文档 L27）；这里给非 Windows 留出口
			sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
		#endif

			auto impl = std::make_unique<Impl>();
			impl->Level = config.Level;
			impl->CaptureStacktraceOnError = config.CaptureStacktraceOnError;
			impl->Instance = std::make_unique<spdlog::logger>("PenFramework", sinks.begin(), sinks.end());

			impl->Instance->set_level(ToBackendLevel(config.Level));
			// 时间 + 等级 + 线程 + 消息；%^/%$ 是颜色起止
			impl->Instance->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%l] [t=%t] %v%$");
			// Warn 以上立刻落盘：崩溃前最后几条必须在文件里（旧件是每条都 flush，代价太大）
			impl->Instance->flush_on(spdlog::level::warn);
			// sink 写失败不要走 spdlog 默认的"每秒最多一条 stderr"：这里显式接管
			impl->Instance->set_error_handler([](const std::string& message)
			{
				std::fwrite(message.data(), 1, message.size(), stderr);
			});

			m_impl = std::move(impl);
			return {};
		}
		catch (const spdlog::spdlog_ex&)
		{
			// spdlog 的构造/打开失败都属于这一类（文件打不开、sink 配置非法等）。
			// 其它异常（如 bad_alloc）不在这里吞掉：OOM 是启动期致命错误，
			// 交给上层异常层处理，不伪装成"文件打开失败"。
			return std::unexpected(LoggerError::FileSinkOpenFailed);
		}
	}

	void Logger::Shutdown() noexcept
	{
		if (!m_impl)
			return;

		try
		{
			if (m_impl->Instance)
				m_impl->Instance->flush();   // 拔掉 sink 之前先落盘
		}
		catch (...)
		{
		}

		m_impl.reset();
	}

	void Logger::Flush() noexcept
	{
		if (!m_impl || !m_impl->Instance)
			return;

		try
		{
			m_impl->Instance->flush();
		}
		catch (...)
		{
		}
	}

	bool Logger::IsInitialized() const noexcept
	{
		return static_cast<bool>(m_impl);
	}

	LoggerLevel Logger::Level() const noexcept
	{
		return m_impl ? m_impl->Level : LoggerLevel::Warn;
	}

	bool Logger::ShouldLog(LoggerLevel level) const noexcept
	{
		// 未初始化：只放行 Warn 以上，交给兜底输出 —— 启动期的错误不会被吞掉
		return m_impl ? level >= m_impl->Level : level >= LoggerLevel::Warn;
	}

	void Logger::LogRaw(LoggerLevel level, std::string_view message) noexcept
	{
		// I6：只在 Error 及以上自动捕栈（成本见 LoggerConfig::CaptureStacktraceOnError 的注释）
		if (m_impl && m_impl->CaptureStacktraceOnError && level >= LoggerLevel::Error)
		{
			try
			{
				// skip = 1：跳过 current() 自身那一帧
				LogRaw(level, message, std::stacktrace::current(1));
				return;
			}
			catch (...)
			{
				// 捕获失败就退回不带栈的记录，不放弃这条日志
			}
		}

		LogRaw(level, message, {});
	}

	void Logger::LogRaw(LoggerLevel level, std::string_view message, const std::stacktrace& stacktrace) noexcept
	{
		if (!m_impl)
		{
			FallbackOutput(level, message);
			return;
		}

		try
		{
			if (stacktrace.empty())
			{
				// 明确走 spdlog 的 string_view 重载：消息不会被当作格式串再解释一次（I4）
				m_impl->Instance->log(ToBackendLevel(level), message);
				return;
			}

			// I7：消息与栈合成**一条**记录 —— 时间/等级/线程前缀只打一次，栈的每帧独占一行。
			// 这里的合成缓冲必须是局部量：message 很可能正指向 Internal::LogScratchBuffer()，
			// 复用它会被 append 的重分配打断。
			std::string composed;
			composed.reserve(message.size() + 256);
			composed.append(message);
			composed.push_back('\n');
			composed.append(std::to_string(stacktrace));

			m_impl->Instance->log(ToBackendLevel(level), std::string_view(composed));
		}
		catch (...)
		{
			// I5：日志路径不抛 —— 一条日志写不出去不该影响游戏逻辑
		}
	}
}
