// File /Native/Engine/Utils/Logger/Logger.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../../IO/Path.h"
#include "../Singleton.hpp"

#include <expected>
#include <format>
#include <iterator>
#include <memory>
#include <stacktrace>
#include <string>
#include <string_view>

// 日志等级的编译期下限（0=Trace .. 6=Off）：低于它的宏在编译期展开为空，
// 因此**实参不会被求值**——这是"Release 里丢掉 Debug 日志"的零成本形式。
// CMake 可用 PENFRAMEWORK_LOG_MIN_LEVEL 覆盖；未定义时按构建配置取值。
#ifndef PENFRAMEWORK_LOG_MIN_LEVEL
	#ifdef PENFRAMEWORK_BUILD_DEBUG
		#define PENFRAMEWORK_LOG_MIN_LEVEL 0
	#else
		#define PENFRAMEWORK_LOG_MIN_LEVEL 2
	#endif
#endif

namespace PenEngine
{
	/// @brief 日志等级；数值即阈值，等级 >= 阈值才输出
	enum class LoggerLevel : U8
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warn = 3,
		Error = 4,
		Critical = 5,
		Off = 6
	};

	/// @brief 初始化失败原因（不抛异常，与仓库既有 std::expected 风格一致）
	enum class LoggerError : U8
	{
		AlreadyInitialized,
		InvalidConfiguration,
		FileSinkOpenFailed
	};

	/// @brief 日志器配置
	struct LoggerConfig
	{
		/// @brief 日志文件（UTF-8 路径）；父目录不存在时由文件后端自行创建
		Path FilePath;

		/// @brief 单文件上限（字节），必须 > 0（0 会让轮转后端每次写都判定超限）
		Usize MaxFileSize = 3 * 1024 * 1024;

		/// @brief 保留的历史文件个数（不含当前文件），必须 > 0
		Usize MaxFileCount = 10;

		/// @brief 过滤阈值；默认随构建配置（Debug: Debug，Release: Info）
	#ifdef PENFRAMEWORK_BUILD_DEBUG
		LoggerLevel Level = LoggerLevel::Debug;
	#else
		LoggerLevel Level = LoggerLevel::Info;
	#endif

		/// @brief 是否挂彩色控制台 sink
		/// @note Native 子系统没有控制台，此处应为 false（改挂调试器输出）。
		///       默认值依据构建期宏 PENFRAMEWORK_BUILD_TERMINAL，因此该宏必须同时
		///       定义在 PenEngine 上（见根 CMakeLists.txt 的日志后端段）
	#ifdef PENFRAMEWORK_BUILD_TERMINAL
		bool EnableConsoleSink = true;
	#else
		bool EnableConsoleSink = false;
	#endif

		/// @brief Error / Critical 是否自动附调用栈
		/// @note 成本（本机实测，捕获+符号化）：Release ≈ 0.014 ms/次，
		///       Debug（/Od）≈ 0.47 ms/次 —— 因此**只对 Error 及以上**开启；
		///       高频等级若要附栈会直接吃掉帧预算。
		/// @note 栈里能否出函数名/文件行取决于该构建是否带 PDB（RelWithDebInfo 形态）；
		///       无 PDB 时退化为 `模块+偏移`，仍可用于事后离线符号化。
		/// @note 关闭后 `Error` / `Critical` 仍是单行记录，只是不带栈。
		bool CaptureStacktraceOnError = true;
	};

	/// @brief 引擎日志器门面
	///
	/// 设计要点（对应旧 `PenFramework-old/Code/Engine/Utils/Logger.hpp` 的问题）：
	///
	/// - **spdlog 是纯实现细节**：本头文件不出现任何 `spdlog/*`，后端全部关在
	///   `Logger.cpp` 的 `Impl` 里。换后端只需改一个 .cpp，宏与调用点不动。
	/// - **格式化在门面内完成**：入口收 `std::format_string<Args...>`，编译期格式检查
	///   由本仓库既有的 `std::formatter` 特化（`String`/`StringView`/`Path`/`PathView`/`URI`）
	///   承担，不依赖 spdlog 的 `format_string_t`（后者在 `__cpp_lib_format < 202207L`
	///   的标准库上会退化成 `std::string_view`，静默丢掉编译期检查）。
	/// - **未初始化不崩**：`Initialize()` 之前的日志按等级分流，Warn 以上走兜底输出，
	///   不复制旧件 `Logger.hpp:54` 那种"未 Init 即空指针解引用"的写法。
	/// - **门面内不抛**：初始化失败返回 `std::expected`；写入路径 `noexcept` 且吞掉异常。
	///
	/// @note 单例形态与 `NotificationBox` 一致（`Singleton<T>` 在前）。
	/// @note `ShouldLog` / `LogRaw` 是宏与模板的唯一入口：将来加分类、异步或环形缓冲，
	///       只改这两个地方与 `.cpp`，调用点不受影响。
	class Logger final : public Singleton<Logger>
	{
	public:
		/// @note 默认构造必须**定义在 .cpp**（不能写成 `= default`）：
		///       否则每个包含本头的 TU 都会实例化 `unique_ptr<Impl>` 的删除器，
		///       而那里只有 `Impl` 的前置声明 → "can't delete an incomplete type"
		Logger() noexcept;
		~Logger() noexcept override;

		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;
		Logger(Logger&&) = delete;
		Logger& operator=(Logger&&) = delete;

		// ---- 生命周期 ----

		/// @brief 初始化后端
		/// @return 成功返回 `void`；失败返回原因（不抛异常）
		std::expected<void, LoggerError> Initialize(const LoggerConfig& config);

		/// @brief 落盘并释放后端；之后可再次 `Initialize()`
		void Shutdown() noexcept;

		/// @brief 立即落盘（崩溃前 / 帧末手动调用）
		void Flush() noexcept;

		[[nodiscard]] bool IsInitialized() const noexcept;

		/// @brief 当前阈值；未初始化时等价于 Warn（与 `ShouldLog` 一致）
		[[nodiscard]] LoggerLevel Level() const noexcept;

		/// @brief 该等级当前是否会输出（模板与宏的唯一早退点）
		[[nodiscard]] bool ShouldLog(LoggerLevel level) const noexcept;

		/// @brief 写入一条**已格式化**的消息（后端唯一入口）
		/// @note 形参是 `std::string_view` 是有意的：spdlog 对 `string_view` 有"不做格式化"
		///       的重载，因此消息里出现的 `{}` 不会被当成格式串二次解释
		/// @note 当 `level >= Error` 且 `CaptureStacktraceOnError` 为真时**自动附栈**；
		///       栈内最上面 2–3 帧是日志器自身（`LogRaw` / `Log` / `Error`），属预期
		void LogRaw(LoggerLevel level, std::string_view message) noexcept;

		/// @brief 写入一条**已格式化**的消息，并附带调用方**已捕获**的栈
		/// @param stacktrace 调用方提供的栈；为空则不附
		/// @note 用途：`catch` 里已经有 `PenEngine::Exception::Stacktrace()`，
		///       直接复用可省掉一次捕获（Debug 下一次约 0.47 ms），不要重新 `current()`
		void LogRaw(LoggerLevel level, std::string_view message, const std::stacktrace& stacktrace) noexcept;

		// ---- 门面入口 ----

		template <typename... Args>
		void Log(LoggerLevel level, std::format_string<Args...> fmt, Args&&... args);

		/// @brief 写入一条消息并附带调用方已捕获的栈（不再自动捕获）
		/// @note 典型用法：顶层 `catch (const PenEngine::Exception& e)` 里
		///       `Logger::GetInstance().LogWithStacktrace(LoggerLevel::Critical, e.Stacktrace(), "...", ...)`
		template <typename... Args>
		void LogWithStacktrace(LoggerLevel level, const std::stacktrace& stacktrace, std::format_string<Args...> fmt, Args&&... args);

		template <typename... Args> void Trace(std::format_string<Args...> fmt, Args&&... args);
		template <typename... Args> void Debug(std::format_string<Args...> fmt, Args&&... args);
		template <typename... Args> void Info(std::format_string<Args...> fmt, Args&&... args);
		template <typename... Args> void Warn(std::format_string<Args...> fmt, Args&&... args);
		template <typename... Args> void Error(std::format_string<Args...> fmt, Args&&... args);
		template <typename... Args> void Critical(std::format_string<Args...> fmt, Args&&... args);

	private:
		/// @brief 后端细节；spdlog 的类型只出现在它的定义处（Logger.cpp）
		struct Impl;

		std::unique_ptr<Impl> m_impl;
	};

	namespace Internal
	{
		/// @brief 逐条日志复用的格式化缓冲（thread_local，避免每条日志一次分配）
		/// @note 不支持可重入：格式化器内部再打日志会覆盖本缓冲
		[[nodiscard]] std::string& LogScratchBuffer();
	}

	template <typename... Args>
	void Logger::Log(LoggerLevel level, std::format_string<Args...> fmt, Args&&... args)
	{
		if (!ShouldLog(level))
			return;

		std::string& buffer = Internal::LogScratchBuffer();
		buffer.clear();
		std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
		LogRaw(level, buffer);
	}

	template <typename... Args>
	void Logger::LogWithStacktrace(LoggerLevel level, const std::stacktrace& stacktrace, std::format_string<Args...> fmt, Args&&... args)
	{
		if (!ShouldLog(level))
			return;

		std::string& buffer = Internal::LogScratchBuffer();
		buffer.clear();
		std::format_to(std::back_inserter(buffer), fmt, std::forward<Args>(args)...);
		LogRaw(level, buffer, stacktrace);
	}

	template <typename... Args>
	void Logger::Trace(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(LoggerLevel::Trace, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Logger::Debug(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(LoggerLevel::Debug, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Logger::Info(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(LoggerLevel::Info, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Logger::Warn(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(LoggerLevel::Warn, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Logger::Error(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(LoggerLevel::Error, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void Logger::Critical(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(LoggerLevel::Critical, fmt, std::forward<Args>(args)...);
	}

	// ================================================================
	// 不变量（改本类实现前先读，改完同步更新）
	//
	//   I1 Initialize 只能成功一次；Shutdown 之后可再次 Initialize
	//   I2 m_impl 仅在 Initialize 成功后非空；LogRaw 在 m_impl 为空时走兜底输出，
	//      绝不解引用它（未初始化打日志不是错误，但也不能崩）
	//   I3 所有 sink 均为 *_mt 变体：日志可从任意线程调用
	//   I4 格式化只在门面完成，交给后端的一律是"不再含格式串语义"的成品字符串
	//   I5 写入路径不抛：Initialize 之外的任何失败都只丢弃这一条日志
	//   I6 自动附栈只发生在 level >= Error 且 CaptureStacktraceOnError 为真时；
	//      调用方自带栈走 LogRaw(3 参) / LogWithStacktrace，绝不二次捕获
	//   I7 消息与栈合成**一条**记录（栈的每帧独占一行）；为此用的合成缓冲是局部量，
	//      不与 Internal::LogScratchBuffer() 混用（后者可能正被 message 引用）
	// ================================================================
}

// ---- 便捷宏（前缀依据 AGENTS §6.4：引擎层新增宏一律 PENFRAMEWORK_*）----
// 低于 PENFRAMEWORK_LOG_MIN_LEVEL 的等级在编译期展开为空 —— 注意副作用：实参不求值
#if PENFRAMEWORK_LOG_MIN_LEVEL <= 0
	#define PENFRAMEWORK_LOG_TRACE(...)    ::PenEngine::Logger::GetInstance().Trace(__VA_ARGS__)
#else
	#define PENFRAMEWORK_LOG_TRACE(...)    ((void)0)
#endif

#if PENFRAMEWORK_LOG_MIN_LEVEL <= 1
	#define PENFRAMEWORK_LOG_DEBUG(...)    ::PenEngine::Logger::GetInstance().Debug(__VA_ARGS__)
#else
	#define PENFRAMEWORK_LOG_DEBUG(...)    ((void)0)
#endif

#if PENFRAMEWORK_LOG_MIN_LEVEL <= 2
	#define PENFRAMEWORK_LOG_INFO(...)     ::PenEngine::Logger::GetInstance().Info(__VA_ARGS__)
#else
	#define PENFRAMEWORK_LOG_INFO(...)     ((void)0)
#endif

#if PENFRAMEWORK_LOG_MIN_LEVEL <= 3
	#define PENFRAMEWORK_LOG_WARN(...)     ::PenEngine::Logger::GetInstance().Warn(__VA_ARGS__)
#else
	#define PENFRAMEWORK_LOG_WARN(...)     ((void)0)
#endif

#if PENFRAMEWORK_LOG_MIN_LEVEL <= 4
	#define PENFRAMEWORK_LOG_ERROR(...)    ::PenEngine::Logger::GetInstance().Error(__VA_ARGS__)
#else
	#define PENFRAMEWORK_LOG_ERROR(...)    ((void)0)
#endif

#if PENFRAMEWORK_LOG_MIN_LEVEL <= 5
	#define PENFRAMEWORK_LOG_CRITICAL(...) ::PenEngine::Logger::GetInstance().Critical(__VA_ARGS__)
#else
	#define PENFRAMEWORK_LOG_CRITICAL(...) ((void)0)
#endif
