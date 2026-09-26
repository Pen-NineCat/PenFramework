// File /Native/Engine/IO/IIODevice.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Coroutine/CoroutineTask.hpp"
#include "../Engine/Core/Environment.h"
#include "../String/String.hpp"
#include "../Utils/ByteArray.hpp"
#include "IInputBuffer.h"
#include "IOutputBuffer.h"
#include <expected>

#define PENFRAMEWORK_IODEVICE_OVERLOADING \
	using PenEngine::IBasicIODevice::ReadTo; \
	using PenEngine::IBasicIODevice::AsyncReadTo; \
	using PenEngine::IBasicIODevice::AsyncWriteAllFrom; \
	using PenEngine::IBasicIODevice::AsyncWriteFrom; \
	using PenEngine::IBasicIODevice::WriteAllFrom; \
	using PenEngine::IBasicIODevice::WriteFrom;

namespace PenEngine
{
	enum class IODeviceOperationResult : U8
	{
		Success,

		InvalidMode, // 打开模式与操作不匹配，例如尝试以只读模式写入数据
		InvalidArgument, // 参数无效

		AccessDenied, // 权限不足，例如没有足够的权限访问文件或目录
		SharingViolation, // 进程无法访问文件，因为另一个进程正在使用该文件
		Busy, // 资源被占用

		TargetAlreadyExists, // 目标已存在，例如尝试创建一个已经存在的文件或目录
		TargetInvalid, // 目标无效，例如目标路径指向一个不存在的文件或目录，或者目标路径格式错误
		TargetNotFound, // 目标未找到，例如尝试访问一个不存在的文件或目录

		FileNotOpen, // 目标未打开，例如尝试对一个未成功打开的文件进行读写操作

		WouldBlock, // 非阻塞操作当前无法完成，需要重试
		Interrupted, // 系统调用被信号阻断，需要重试

		BufferFull, // 目标缓冲区已满，无法写入数据

		IOFailure, // 输入输出错误
		OSFailure, // 操作系统调用失败

		NotSupported, // 操作系统或者设备不支持该操作

		AsyncTaskRunning, // 正在执行异步任务
		AsyncTaskCancelled, // 异步任务被取消

		StreamEnd, // 缓冲区已经读完

		UnknownError // 未知错误
	};

	/// @brief 简单IO设备接口
	class PENFRAMEWORK_NO_VTABLE IBasicIODevice
	{
	public:
		virtual ~IBasicIODevice() noexcept = default;

		virtual IODeviceOperationResult WriteFrom(const U8* buf, Usize size) = 0;
		virtual CoroutineTask<IODeviceOperationResult> AsyncWriteFrom(const U8* buf, Usize size, bool copyToInternal = true) = 0;

		virtual std::expected<Usize, IODeviceOperationResult> ReadTo(U8* buf, Usize size) = 0;
		virtual CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadTo(U8* buf, Usize size) = 0;

		IODeviceOperationResult WriteFrom(const ByteArray& buf, Usize size)
		{
			return WriteFrom(buf.Data(), std::min(buf.Size(), size));
		}

		IODeviceOperationResult WriteFrom(const IOutputBuffer& buf, Usize size)
		{
			return WriteFrom(buf.Data(), std::min(buf.Size(), size));
		}

		IODeviceOperationResult WriteFrom(const std::vector<U8>& buf, Usize size)
		{
			return WriteFrom(buf.data(), std::min(buf.size(), size));
		}

		IODeviceOperationResult WriteAllFrom(const ByteArray& buf, Usize size)
		{
			return WriteFrom(buf.Data(), buf.Size());
		}

		IODeviceOperationResult WriteAllFrom(const IOutputBuffer& buf, Usize size)
		{
			return WriteFrom(buf.Data(), buf.Size());
		}

		IODeviceOperationResult WriteAllFrom(const std::vector<U8>& buf, Usize size)
		{
			return WriteFrom(buf.data(), buf.size());
		}

		CoroutineTask<IODeviceOperationResult> AsyncWriteFrom(const ByteArray& buf, Usize size, bool copyToInternal = true)
		{
			return AsyncWriteFrom(buf.Data(), std::min(buf.Size(), size), copyToInternal);
		}

		CoroutineTask<IODeviceOperationResult> AsyncWriteFrom(const IOutputBuffer& buf, Usize size, bool copyToInternal = true)
		{
			return AsyncWriteFrom(buf.Data(), std::min(buf.Size(), size), copyToInternal);
		}

		CoroutineTask<IODeviceOperationResult> AsyncWriteFrom(const std::vector<U8>& buf, Usize size, bool copyToInternal = true)
		{
			return AsyncWriteFrom(buf.data(), std::min(buf.size(), size), copyToInternal);
		}

		CoroutineTask<IODeviceOperationResult> AsyncWriteAllFrom(const ByteArray& buf, bool copyToInternal = true)
		{
			return AsyncWriteFrom(buf.Data(), buf.Size(), copyToInternal);
		}

		CoroutineTask<IODeviceOperationResult> AsyncWriteAllFrom(const IOutputBuffer& buf, bool copyToInternal = true)
		{
			return AsyncWriteFrom(buf.Data(), buf.Size(), copyToInternal);
		}

		CoroutineTask<IODeviceOperationResult> AsyncWriteAllFrom(const std::vector<U8>& buf, bool copyToInternal = true)
		{
			return AsyncWriteFrom(buf.data(), buf.size(), copyToInternal);
		}

		std::expected<Usize, IODeviceOperationResult> ReadTo(IInputBuffer& buf, Usize size)
		{
			if (size == 0)
				return 0;

			U8* destination = buf.PrepareBuffer(size);
			if (destination == nullptr)
				return std::unexpected(IODeviceOperationResult::InvalidArgument);

			std::expected<Usize, IODeviceOperationResult> result = ReadTo(
				destination, size);

			if (result.has_value())
				buf.SetActualBufferLen(result.value());

			return result;
		}

		std::expected<Usize, IODeviceOperationResult> ReadTo(ByteArray& buf, Usize size)
		{
			if (size == 0)
				return 0;

			buf.Reserve(size);

			std::expected<Usize, IODeviceOperationResult> result = ReadTo(
				buf.Data(), size);

			if (result.has_value())
				buf.Resize(result.value());

			return result;
		}

		std::expected<Usize, IODeviceOperationResult> ReadTo(std::vector<U8>& buf, Usize size)
		{
			if (size == 0)
				return 0;

			buf.reserve(size);

			std::expected<Usize, IODeviceOperationResult> result = ReadTo(
				buf.data(), size);

			if (result.has_value())
				buf.resize(result.value());

			return result;
		}

		CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadTo(ByteArray& buf, Usize size)
		{
			if (size == 0)
				co_return 0;

			buf.Reserve(size);

			std::expected<Usize, IODeviceOperationResult> result = co_await AsyncReadTo(
				buf.Data(), size);

			if (result.has_value())
				buf.Resize(result.value());

			co_return result;
		}

		CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadTo(std::vector<U8>& buf, Usize size)
		{
			if (size == 0)
				co_return 0;

			buf.reserve(size);

			std::expected<Usize, IODeviceOperationResult> result = co_await AsyncReadTo(
				buf.data(), size);

			if (result.has_value())
				buf.resize(result.value());

			co_return result;
		}

		CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadTo(IInputBuffer& buf, Usize size)
		{
			if (size == 0)
				co_return 0;

			U8* destination = buf.PrepareBuffer(size);
			if (destination == nullptr)
				co_return std::unexpected(IODeviceOperationResult::InvalidArgument);

			std::expected<Usize, IODeviceOperationResult> result = co_await AsyncReadTo(destination, size);
			if (result.has_value())
				buf.SetActualBufferLen(result.value());

			co_return result;
		}
	};
}
