// File /Native/Engine/Test/UnitTest/LoggerTest.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// Logger 门面的验收用例：只走公开 API（不碰 spdlog），断言"写进文件的到底是什么"。
// 日志文件放在系统临时目录，不污染构建输出目录。

#include "Engine/Utils/Logger/Logger.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stacktrace>
#include <string>
#include <string_view>

using namespace PenEngine;

namespace
{
	/// @brief 每个用例独立的日志路径（同目录下按用例名区分）
	[[nodiscard]] Path MakeLogPath(const std::string& name)
	{
		return Path(std::filesystem::temp_directory_path()) / "PenFrameworkLoggerTest" / (name + ".log");
	}

	/// @brief 读回整个日志文件；读不到返回空串（用例自己断言内容，失败会明确报出来）
	[[nodiscard]] std::string ReadLogFile(const Path& path)
	{
		std::ifstream stream(path.Data(), std::ios::binary);
		if (!stream)
			return {};

		std::ostringstream buffer;
		buffer << stream.rdbuf();
		return buffer.str();
	}

	/// @brief 清理用例产物（轮转会额外生成 .1/.2 这类历史文件）
	void RemoveLogFiles(const Path& path)
	{
		std::error_code error;
		const std::string base = path.Data();
		std::filesystem::remove(base, error);
		std::filesystem::remove(base + ".1", error);
		std::filesystem::remove(base + ".2", error);
	}

	/// @brief 日志文件的行数（每条记录至少一行；带栈的记录会被撑成多行）
	[[nodiscard]] std::size_t CountLines(std::string_view content) noexcept
	{
		std::size_t lines = 0;
		for (const char character : content)
		{
			if (character == '\n')
				++lines;
		}
		return lines;
	}
}

/// @brief 日志器是进程级单例：每个用例前后都归零，避免用例之间互相影响
class LoggerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		Logger::GetInstance().Shutdown();
	}

	void TearDown() override
	{
		Logger::GetInstance().Shutdown();
	}
};

TEST_F(LoggerTest, WritesFormattedMessageWithUtf8Payload)
{
	const Path logPath = MakeLogPath("WritesFormattedMessageWithUtf8Payload");
	RemoveLogFiles(logPath);

	LoggerConfig config;
	config.FilePath = logPath;
	config.Level = LoggerLevel::Trace;
	config.EnableConsoleSink = false;   // 用例不产生控制台噪音

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	PENFRAMEWORK_LOG_INFO("数值 {} / 中文 {}", 42, "日志");
	Logger::GetInstance().Flush();

	const std::string content = ReadLogFile(logPath);
	EXPECT_NE(content.find("数值 42 / 中文 日志"), std::string::npos) << "文件内容：\n" << content;
	EXPECT_NE(content.find("[info]"), std::string::npos) << "文件内容：\n" << content;
	// 模式串真的被应用了（%t = 线程号）；否则上面两条仍可能通过
	EXPECT_NE(content.find("[t="), std::string::npos) << "文件内容：\n" << content;

	// ctest -V 下把真实日志行显示出来，便于人肉核对格式
	std::cout << "[captured log line] " << content;

	RemoveLogFiles(logPath);
}

TEST_F(LoggerTest, FiltersBelowConfiguredLevel)
{
	const Path logPath = MakeLogPath("FiltersBelowConfiguredLevel");
	RemoveLogFiles(logPath);

	LoggerConfig config;
	config.FilePath = logPath;
	config.Level = LoggerLevel::Warn;
	config.EnableConsoleSink = false;

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	PENFRAMEWORK_LOG_INFO("info-should-be-filtered");
	PENFRAMEWORK_LOG_WARN("warn-should-be-kept");
	Logger::GetInstance().Flush();

	const std::string content = ReadLogFile(logPath);
	EXPECT_EQ(content.find("info-should-be-filtered"), std::string::npos) << "文件内容：\n" << content;
	EXPECT_NE(content.find("warn-should-be-kept"), std::string::npos) << "文件内容：\n" << content;

	RemoveLogFiles(logPath);
}

TEST_F(LoggerTest, MessageBracesAreNotReinterpreted)
{
	const Path logPath = MakeLogPath("MessageBracesAreNotReinterpreted");
	RemoveLogFiles(logPath);

	LoggerConfig config;
	config.FilePath = logPath;
	config.Level = LoggerLevel::Trace;
	config.EnableConsoleSink = false;

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	// 成品消息里带花括号：必须在后端被当作**普通文本**（I4），而不是二次格式化而抛异常
	PENFRAMEWORK_LOG_INFO("{}", "raw {braces} and {}");
	Logger::GetInstance().Flush();

	const std::string content = ReadLogFile(logPath);
	EXPECT_NE(content.find("raw {braces} and {}"), std::string::npos) << "文件内容：\n" << content;

	RemoveLogFiles(logPath);
}

TEST_F(LoggerTest, UninitializedLoggerDoesNotCrashAndKeepsWarnVisible)
{
	// SetUp 已 Shutdown：此刻是"未初始化"状态
	EXPECT_FALSE(Logger::GetInstance().IsInitialized());
	EXPECT_EQ(Logger::GetInstance().Level(), LoggerLevel::Warn);
	EXPECT_FALSE(Logger::GetInstance().ShouldLog(LoggerLevel::Info));
	EXPECT_TRUE(Logger::GetInstance().ShouldLog(LoggerLevel::Warn));

	// 这两行分别走"被过滤"与"兜底输出（stderr）"两条路径，均不得崩溃
	PENFRAMEWORK_LOG_INFO("dropped-before-initialize");
	PENFRAMEWORK_LOG_WARN("fallback-before-initialize");
}

TEST_F(LoggerTest, RejectsInvalidConfiguration)
{
	LoggerConfig emptyPath;
	emptyPath.FilePath = Path();
	emptyPath.EnableConsoleSink = false;

	const auto emptyResult = Logger::GetInstance().Initialize(emptyPath);
	ASSERT_FALSE(emptyResult.has_value());
	EXPECT_EQ(emptyResult.error(), LoggerError::InvalidConfiguration);

	LoggerConfig zeroSize;
	zeroSize.FilePath = MakeLogPath("RejectsInvalidConfiguration");
	zeroSize.MaxFileSize = 0;
	zeroSize.EnableConsoleSink = false;

	const auto zeroSizeResult = Logger::GetInstance().Initialize(zeroSize);
	ASSERT_FALSE(zeroSizeResult.has_value());
	EXPECT_EQ(zeroSizeResult.error(), LoggerError::InvalidConfiguration);

	EXPECT_FALSE(Logger::GetInstance().IsInitialized());
}

TEST_F(LoggerTest, SecondInitializeRejectedAndReinitializeAfterShutdownWorks)
{
	const Path first = MakeLogPath("SecondInitializeRejected");
	const Path second = MakeLogPath("ReinitializeAfterShutdown");
	RemoveLogFiles(first);
	RemoveLogFiles(second);

	LoggerConfig config;
	config.FilePath = first;
	config.Level = LoggerLevel::Trace;
	config.EnableConsoleSink = false;

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	// I1：成功一次之后再次 Initialize 必须被拒绝，且不破坏既有后端
	const auto secondResult = Logger::GetInstance().Initialize(config);
	ASSERT_FALSE(secondResult.has_value());
	EXPECT_EQ(secondResult.error(), LoggerError::AlreadyInitialized);

	PENFRAMEWORK_LOG_INFO("first-session");
	Logger::GetInstance().Flush();
	EXPECT_NE(ReadLogFile(first).find("first-session"), std::string::npos);

	// I1：Shutdown 之后可以重新初始化到另一个文件
	Logger::GetInstance().Shutdown();
	EXPECT_FALSE(Logger::GetInstance().IsInitialized());

	config.FilePath = second;
	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	PENFRAMEWORK_LOG_INFO("second-session");
	Logger::GetInstance().Flush();

	const std::string secondContent = ReadLogFile(second);
	EXPECT_NE(secondContent.find("second-session"), std::string::npos) << "文件内容：\n" << secondContent;
	EXPECT_EQ(secondContent.find("first-session"), std::string::npos) << "新会话不应写入旧文件";

	RemoveLogFiles(first);
	RemoveLogFiles(second);
}

TEST_F(LoggerTest, AttachesStacktraceToErrorButNotToInfo)
{
	const Path logPath = MakeLogPath("AttachesStacktraceToErrorButNotToInfo");
	RemoveLogFiles(logPath);

	LoggerConfig config;
	config.FilePath = logPath;
	config.Level = LoggerLevel::Trace;
	config.EnableConsoleSink = false;   // 捕获行为用默认值（默认开启）

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	PENFRAMEWORK_LOG_INFO("info-single-line");
	PENFRAMEWORK_LOG_ERROR("error-with-stack");
	Logger::GetInstance().Flush();

	const std::string content = ReadLogFile(logPath);
	EXPECT_NE(content.find("info-single-line"), std::string::npos) << "文件内容：\n" << content;
	EXPECT_NE(content.find("error-with-stack"), std::string::npos) << "文件内容：\n" << content;

	// Info 一条记录占一行；Error 带栈 -> 至少多出若干行。符号名有无取决于 PDB，故只断言"多行"。
	EXPECT_GE(CountLines(content), 3u) << "Error 未附栈？文件内容：\n" << content;

	std::cout << "[captured error with stack]\n" << content;

	RemoveLogFiles(logPath);
}

TEST_F(LoggerTest, StacktraceCaptureCanBeDisabled)
{
	const Path logPath = MakeLogPath("StacktraceCaptureCanBeDisabled");
	RemoveLogFiles(logPath);

	LoggerConfig config;
	config.FilePath = logPath;
	config.Level = LoggerLevel::Trace;
	config.EnableConsoleSink = false;
	config.CaptureStacktraceOnError = false;

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	PENFRAMEWORK_LOG_INFO("info-single-line");
	PENFRAMEWORK_LOG_ERROR("error-single-line");
	Logger::GetInstance().Flush();

	const std::string content = ReadLogFile(logPath);
	EXPECT_NE(content.find("error-single-line"), std::string::npos) << "文件内容：\n" << content;
	EXPECT_EQ(CountLines(content), 2u) << "关闭捕获后 Error 不应附栈。文件内容：\n" << content;

	RemoveLogFiles(logPath);
}

TEST_F(LoggerTest, LogWithStacktraceRendersCallerProvidedTrace)
{
	const Path logPath = MakeLogPath("LogWithStacktraceRendersCallerProvidedTrace");
	RemoveLogFiles(logPath);

	LoggerConfig config;
	config.FilePath = logPath;
	config.Level = LoggerLevel::Trace;
	config.EnableConsoleSink = false;
	config.CaptureStacktraceOnError = false;   // 证明多出来的行确实来自调用方提供的栈

	ASSERT_TRUE(Logger::GetInstance().Initialize(config).has_value());

	// 模拟 catch 里复用 PenEngine::Exception::Stacktrace() 的用法
	const std::stacktrace trace = std::stacktrace::current();
	Logger::GetInstance().LogWithStacktrace(LoggerLevel::Error, trace, "caller-provided-stack {}", 7);
	Logger::GetInstance().Flush();

	const std::string content = ReadLogFile(logPath);
	EXPECT_NE(content.find("caller-provided-stack 7"), std::string::npos) << "文件内容：\n" << content;
	EXPECT_GE(CountLines(content), 3u) << "调用方提供的栈未被渲染。文件内容：\n" << content;

	std::cout << "[captured caller trace]\n" << content;

	RemoveLogFiles(logPath);
}
