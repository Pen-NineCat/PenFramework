// File /Native/Engine/IO/Win32FileDevice.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Win32FileDevice.h"

#include "../../OS/Windows/Windows.h"
#include "../../Utils/ResourceScopeGuard.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <io.h>
#include <shellapi.h>

namespace PenEngine
{
	namespace FileDeviceDetail
	{
		/// @brief 把 Win32 错误码映射为设备错误码
		[[nodiscard]] IODeviceOperationResult MapWin32Error(DWORD error) noexcept
		{
			switch (error)
			{
				case ERROR_DISK_FULL:
				case ERROR_HANDLE_DISK_FULL:
					return IODeviceOperationResult::BufferFull;
				case ERROR_ACCESS_DENIED:
					return IODeviceOperationResult::AccessDenied;
				case ERROR_SHARING_VIOLATION:
					return IODeviceOperationResult::SharingViolation;
				case ERROR_NOT_ENOUGH_MEMORY:
				case ERROR_OUTOFMEMORY:
					return IODeviceOperationResult::OSFailure;
				default:
					return IODeviceOperationResult::IOFailure;
			}
		}

		/// @brief 该错误是否表示"读到文件末尾"
		[[nodiscard]] bool IsStreamEndError(DWORD error) noexcept
		{
			return error == ERROR_HANDLE_EOF;
		}

		// 递归删除目录树 / 删除单个文件的内部实现（供 Remove 系列使用）
		[[nodiscard]] bool RemoveRecursive(const Path& target) noexcept
		{
			std::wstring nativePath = target.ToString<wchar_t>().ConvertToStdString<wchar_t>();
			DWORD attribute = GetFileAttributesW(nativePath.c_str());

			if (attribute == INVALID_FILE_ATTRIBUTES)
				return false;

			// 只读属性会阻止删除，先清除
			if (attribute & FILE_ATTRIBUTE_READONLY)
				SetFileAttributesW(nativePath.c_str(), attribute & ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY));

			if ((attribute & FILE_ATTRIBUTE_DIRECTORY) == 0)
				return DeleteFileW(nativePath.c_str()) != FALSE;

			std::wstring pattern = nativePath;
			if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/')
				pattern.push_back(L'\\');
			pattern.append(L"*");

			WIN32_FIND_DATAW findData = {};
			HANDLE findHandle = FindFirstFileW(pattern.c_str(), &findData);

			if (findHandle != INVALID_HANDLE_VALUE)
			{
				do
				{
					const std::wstring_view entryName = findData.cFileName;
					if (entryName == L"." || entryName == L"..")
						continue;

					std::wstring childPath = nativePath;
					if (!childPath.empty() && childPath.back() != L'\\' && childPath.back() != L'/')
						childPath.push_back(L'\\');
					childPath.append(entryName);

					const DWORD childAttribute = findData.dwFileAttributes;
					if (childAttribute & FILE_ATTRIBUTE_READONLY)
						SetFileAttributesW(childPath.c_str(), childAttribute & ~static_cast<DWORD>(FILE_ATTRIBUTE_READONLY));

					if ((childAttribute & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						// 子项可能是目录，需要递归处理（路径已是本机编码，直接转换避免二次规范化）
						Path child;
						child.ConvertDirect(childPath.c_str());
						if (!RemoveRecursive(child))
						{
							FindClose(findHandle);
							return false;
						}
					}
					else if (DeleteFileW(childPath.c_str()) == FALSE)
					{
						FindClose(findHandle);
						return false;
					}
				} while (FindNextFileW(findHandle, &findData) != FALSE);

				FindClose(findHandle);
			}

			return RemoveDirectoryW(nativePath.c_str()) != FALSE;
		}
	}
	IODeviceOperationResult Win32FileDevice::Open(const Path& path, ModeFlag mode)
	{
		if (m_open && Close() != IODeviceOperationResult::Success)
			return IODeviceOperationResult::IOFailure;

		if (mode == Mode::None)
			return IODeviceOperationResult::InvalidMode;

		if (path.Empty())
			return IODeviceOperationResult::InvalidArgument;

		// 保存目标路径：后续 CreateFile 与目录操作均以 m_path 为准
		m_path = path;

		if (mode & (Mode::Append | Mode::NewOnly))
			mode |= Mode::CanWrite;

		DWORD sharedMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

		DWORD accessRights = 0;
		if (mode & Mode::CanRead)
			//GENERIC_WRITE ≈ STANDARD_RIGHTS_WRITE |
			//FILE_WRITE_DATA |
			//FILE_WRITE_ATTRIBUTES |
			//FILE_WRITE_EA |
			//FILE_APPEND_DATA |
			//SYNCHRONIZE
			accessRights |= GENERIC_READ;
		if (mode & Mode::CanWrite)
			//GENERIC_READ ≈ STANDARD_RIGHTS_READ |
			//FILE_READ_DATA |
			//FILE_READ_ATTRIBUTES |
			//FILE_READ_EA |
			//STANDARD_RIGHTS_READ |
			//SYNCHRONIZE
			accessRights |= GENERIC_WRITE;

		DWORD creation = 0;
		if (mode & Mode::NewOnly)
			creation = CREATE_NEW;
		else
		{
			if (mode & Mode::CanWrite && !(mode & Mode::ExistingOnly))
				creation = OPEN_ALWAYS;
			else
				creation = OPEN_EXISTING;
		}

		DWORD attribute = FILE_ATTRIBUTE_NORMAL;
		if (mode & Mode::EnableAsyncOperation)
		{
			attribute |= FILE_FLAG_OVERLAPPED;

			// 重叠句柄上的同步读写需要一个事件来等待完成，整个设备复用一个手动重置事件
			if (m_overlapEvent == nullptr || m_overlapEvent == INVALID_HANDLE_VALUE)
			{
				m_overlapEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
				if (m_overlapEvent == nullptr)
					return IODeviceOperationResult::OSFailure;
			}
		}

		HANDLE createdHandle = CreateFile(m_path.ToString<wchar_t>().Data(), accessRights, sharedMode, nullptr, creation, attribute, nullptr);
		if (createdHandle == INVALID_HANDLE_VALUE)
		{
			switch (GetLastError())
			{
				case ERROR_FILE_NOT_FOUND:
				case ERROR_PATH_NOT_FOUND:
					return IODeviceOperationResult::TargetNotFound;
				case ERROR_ACCESS_DENIED:
					return IODeviceOperationResult::AccessDenied;
				case ERROR_SHARING_VIOLATION:
					return IODeviceOperationResult::SharingViolation;
				case ERROR_INVALID_PARAMETER:
					return IODeviceOperationResult::InvalidMode;
				case ERROR_INVALID_NAME:
					return IODeviceOperationResult::InvalidArgument;
				case ERROR_FILE_EXISTS:
					return IODeviceOperationResult::TargetAlreadyExists;
				default:
					return IODeviceOperationResult::UnknownError;
			}
		}

		m_handle.Win32Handle = createdHandle;

		// 以追加模式打开时，将文件指针移动到文件末尾
		if (mode & Mode::Append)
		{
			LARGE_INTEGER zero{ .QuadPart = 0 };
			if (!SetFilePointerEx(m_handle.Win32Handle, zero, nullptr, FILE_END))
			{
				CloseHandle(m_handle.Win32Handle);
				m_handle.Win32Handle = nullptr;
				return IODeviceOperationResult::IOFailure;
			}
		}

		// 截断必须在句柄生效后立即进行，保证后续写入从 0 字节开始
		if (mode & Mode::Truncate)
		{
			LARGE_INTEGER zero{ .QuadPart = 0 };
			if (!SetFilePointerEx(m_handle.Win32Handle, zero, nullptr, FILE_BEGIN) || !SetEndOfFile(m_handle.Win32Handle))
			{
				CloseHandle(m_handle.Win32Handle);
				m_handle.Win32Handle = nullptr;
				return IODeviceOperationResult::IOFailure;
			}
		}

		// 新会话开始，清空上一次打开遗留的缓冲数据
		m_buffer.Clear();

		m_handleType = FileHandleType::Win;
		m_open = true;
		m_mode = mode;

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::Open(FILE* handle, ModeFlag mode)
	{
		if (m_open && Close() != IODeviceOperationResult::Success)
			return IODeviceOperationResult::IOFailure;

		if (mode == Mode::None || mode & Mode::EnableAsyncOperation)
			return IODeviceOperationResult::InvalidMode;

		if (handle == nullptr)
			return IODeviceOperationResult::InvalidArgument;

		m_buffer.Clear();

		m_handle.PxHandle = handle;
		m_handleType = FileHandleType::Px;
		// 必须同步记录模式，否则后续 WriteFrom/Flush 会因模式为 None 而返回 InvalidMode
		m_mode = mode;
		m_open = true;

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::Close() noexcept
	{
		if (!m_open)
			return IODeviceOperationResult::Success;

		if (m_working)
			return IODeviceOperationResult::AsyncTaskRunning;

		// 刷新前台缓冲区，将尚未落盘的数据同步写入。
		// 此处不修改 m_open：Flush 内部需要看到仍然打开的状态。
		if (!m_buffer.Empty())
		{
			if (IODeviceOperationResult res = Flush(); res != IODeviceOperationResult::Success)
				return res;
		}

		if (m_handleType == FileHandleType::Win)
		{
			if (m_handle.Win32Handle != nullptr && m_handle.Win32Handle != INVALID_HANDLE_VALUE && CloseHandle(m_handle.Win32Handle) == FALSE)
				return IODeviceOperationResult::IOFailure;

			m_handle.Win32Handle = nullptr;
			m_handleType = FileHandleType::Invalid;
		}
		else if (m_handleType == FileHandleType::Px)
		{
			// DontClosePosixHandle 指示仅刷新缓冲区而不接管 FILE* 的生命周期
			if (m_handle.PxHandle != nullptr && !(m_mode & Mode::DontClosePosixHandle))
			{
				if (fclose(m_handle.PxHandle) != 0)
				{
					m_handle.PxHandle = nullptr;
					m_handleType = FileHandleType::Invalid;
					return IODeviceOperationResult::IOFailure;
				}

				m_handle.PxHandle = nullptr;
			}

			m_handleType = FileHandleType::Invalid;
		}

		// 重叠事件属于设备自身资源，无论当前是哪种句柄类型都必须释放
		if (m_overlapEvent != nullptr && m_overlapEvent != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_overlapEvent);
			m_overlapEvent = nullptr;
		}

		m_open = false;
		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::WriteFrom(const U8* buf, Usize size)
	{
		if (buf == nullptr)
			return IODeviceOperationResult::InvalidArgument;

		if (!IsOpen())
			return IODeviceOperationResult::FileNotOpen;

		if (m_working)
			return IODeviceOperationResult::AsyncTaskRunning;

		return WriteFromInternal(buf, size);
	}

	IODeviceOperationResult Win32FileDevice::WriteFromInternal(const U8* buf, Usize size)
	{
		if (buf == nullptr)
			return IODeviceOperationResult::InvalidArgument;

		if (!IsOpen())
			return IODeviceOperationResult::FileNotOpen;

		if (!(m_mode & Mode::CanWrite))
			return IODeviceOperationResult::InvalidMode;

		if (size == 0)
			return IODeviceOperationResult::Success;

		const bool useBuffer = !(m_mode & Mode::DontUseBuffer);

		if (!useBuffer || size > m_syncBufferSize)
		{
			// 直写：先确保先前缓冲的数据先于本次数据落盘
			if (IODeviceOperationResult res = FlushInternal(); res != IODeviceOperationResult::Success)
				return res;

			return InternalSyncWrite(buf, size);
		}

		// 需要先判断合并后是否会越过缓冲区上限，否则会出现"先刷新再写入"导致的双次系统调用
		if (m_buffer.Size() + size > m_syncBufferSize)
		{
			if (IODeviceOperationResult res = FlushInternal(); res != IODeviceOperationResult::Success)
				return res;
		}

		m_buffer.Reserve(m_buffer.Size() + size);
		m_buffer.Append(buf, size);

		return IODeviceOperationResult::Success;
	}

	std::expected<Usize, IODeviceOperationResult> Win32FileDevice::ReadTo(U8* buf, Usize size)
	{
		if (buf == nullptr)
			return std::unexpected(IODeviceOperationResult::InvalidArgument);

		if (!IsOpen())
			return std::unexpected(IODeviceOperationResult::FileNotOpen);

		if (m_working)
			return std::unexpected(IODeviceOperationResult::AsyncTaskRunning);

		if (!(m_mode & Mode::CanRead))
			return std::unexpected(IODeviceOperationResult::InvalidMode);

		// 读取前确保所有已缓冲数据都已落盘，避免读到磁盘上的旧数据
		if (!m_buffer.Empty())
			if (IODeviceOperationResult flushResult = Flush(); flushResult != IODeviceOperationResult::Success)
				return std::unexpected(flushResult);

		if (size == 0)
			return 0;

		return InternalSyncRead(buf, size);
	}

	std::expected<Usize, IODeviceOperationResult> Win32FileDevice::ReadAllTo(IInputBuffer& buf)
	{
		std::expected<Usize, IODeviceOperationResult> fileSize = FileSize();
		if (!fileSize.has_value())
			return std::unexpected(fileSize.error());
			
		return ReadTo(buf, fileSize.value());
	}

	std::expected<Usize, IODeviceOperationResult> Win32FileDevice::ReadAllTo(ByteArray& buf)
	{
		std::expected<Usize, IODeviceOperationResult> fileSize = FileSize();
		if (!fileSize.has_value())
			return std::unexpected(fileSize.error());

		return ReadTo(buf, fileSize.value());
	}

	IODeviceOperationResult Win32FileDevice::Flush() noexcept
	{
		return FlushInternal();
	}

	IODeviceOperationResult Win32FileDevice::FlushInternal() noexcept
	{
		if (!m_open)
			return IODeviceOperationResult::FileNotOpen;

		if (m_working)
			return IODeviceOperationResult::AsyncTaskRunning;

		// 类似于以Mode CanWrite但是传入仅'r'的错误FILE*，会返回InvalidMode
		if (!(m_mode & Mode::CanWrite))
			return IODeviceOperationResult::InvalidMode;

		if (m_buffer.Empty())
			return IODeviceOperationResult::Success;

		if (IODeviceOperationResult ret = InternalSyncWrite(m_buffer.Data(), m_buffer.Size()); ret != IODeviceOperationResult::Success)
			return ret;

		if (m_handleType == FileHandleType::Px && m_handle.PxHandle != nullptr && fflush(m_handle.PxHandle) != 0)
		{
			clearerr(m_handle.PxHandle);
			switch (errno)
			{
				case EBADF:
					return IODeviceOperationResult::InvalidMode;
				default:
					return IODeviceOperationResult::IOFailure;
			}
		}

		m_buffer.Clear();
		return IODeviceOperationResult::Success;
	}

	bool Win32FileDevice::IsOpen() const noexcept
	{
		return m_open;
	}

	void Win32FileDevice::SetSyncBufferSize(Usize bufSize) noexcept
	{
		m_syncBufferSize = bufSize;
	}

	Usize Win32FileDevice::GetSyncBufferSize() const noexcept
	{
		return m_syncBufferSize;
	}

	// ------------------------------------------------------------------
	// 重叠批次：阶段一只判断完成，阶段二才取回结果
	// ------------------------------------------------------------------

	Win32FileDevice::OverlapBatch::BlockContext* Win32FileDevice::OverlapBatch::BlockAt(Usize index) noexcept
	{
		return m_blocks + index;
	}

	const Win32FileDevice::OverlapBatch::BlockContext* Win32FileDevice::OverlapBatch::BlockAt(Usize index) const noexcept
	{
		return m_blocks + index;
	}

	bool Win32FileDevice::OverlapBatch::HasInFlightBlock() const noexcept
	{
		return m_collectIndex < m_launchedCount;
	}

	bool Win32FileDevice::OverlapBatch::AcquireBlockEvents() noexcept
	{
		for (Usize index = 0; index < MAX_ASYNC_OVERLAP_BLOCK; ++index)
		{
			BlockContext* block = BlockAt(index);

			if (block->Overlapped.hEvent != nullptr)
				continue;

			// 每块独立事件：文档明确不建议多路并发重叠操作共用/缺省事件对象
			const HANDLE event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
			if (event == nullptr)
			{
				ReleaseBlockEvents();
				return false;
			}

			block->Overlapped.hEvent = event;
			block->Ready = true;
		}

		return true;
	}

	void Win32FileDevice::OverlapBatch::ReleaseBlockEvents() noexcept
	{
		for (Usize index = 0; index < MAX_ASYNC_OVERLAP_BLOCK; ++index)
		{
			BlockContext* block = BlockAt(index);

			if (block->Overlapped.hEvent == nullptr)
				continue;

			CloseHandle(block->Overlapped.hEvent);
			block->Overlapped.hEvent = nullptr;
		}
	}

	Win32FileDevice::OverlapBatch::~OverlapBatch() noexcept
	{
		Cancel();
		ReleaseBlockEvents();
	}

	bool Win32FileDevice::OverlapBatch::Cancel()
	{
		if (m_launchedCount == 0)
			return true;

		bool anyInFlight = false;
		for (Usize index = 0; index < m_launchedCount; ++index)
		{
			if (BlockAt(index)->State == OverlapBlockState::InFlight)
			{
				anyInFlight = true;
				break;
			}
		}

		if (!anyInFlight)
		{
			m_collectIndex = m_launchedCount;
			return true;
		}

		// 取消本线程在目标句柄上尚未完成的重叠操作
		if (m_targetHandle != nullptr && m_targetHandle != INVALID_HANDLE_VALUE)
			(void)CancelIoEx(m_targetHandle, nullptr);

		// 逐块等待落地：被取消的块以ERROR_OPERATION_ABORTED完成。
		// 必须先确保内核不再引用本对象的OVERLAPPED，才允许协程帧被销毁。
		for (Usize index = 0; index < m_launchedCount; ++index)
		{
			BlockContext* block = BlockAt(index);

			if (block->State != OverlapBlockState::InFlight)
				continue;

			DWORD transferred = 0;
			(void)GetOverlappedResult(m_targetHandle, &block->Overlapped, &transferred, TRUE);
			block->State = OverlapBlockState::Finished;
		}

		if (m_error == IODeviceOperationResult::Success)
			m_error = IODeviceOperationResult::AsyncTaskCancelled;

		m_collectIndex = m_launchedCount;
		return true;
	}

	bool Win32FileDevice::OverlapBatch::IsReady() noexcept
	{
		// 阶段一：只询问完成状态，绝不消费结果
		for (Usize index = 0; index < m_launchedCount; ++index)
		{
			BlockContext* block = BlockAt(index);

			if (block->State != OverlapBlockState::InFlight)
				continue;

			// 文档要求：仅在对该操作返回过ERROR_IO_PENDING时才可使用该宏
			if (HasOverlappedIoCompleted(&block->Overlapped))
				block->State = OverlapBlockState::Finished;
			else
				return false;
		}

		return true;
	}

	IODeviceOperationResult Win32FileDevice::OverlapBatch::Launch(bool isWrite, HANDLE handle,
		const U8* buffer, Usize totalSize, U64 beginOffset, Usize& outLaunchedSize) noexcept
	{
		outLaunchedSize = 0;

		if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
			return IODeviceOperationResult::FileNotOpen;

		if (totalSize == 0)
			return IODeviceOperationResult::Success;

		DEBUG_VERIFY_REPORT(m_collectIndex >= m_launchedCount, "OverlapBatch reused before CollectResult");

		if (!AcquireBlockEvents())
			return IODeviceOperationResult::OSFailure;

		m_targetHandle = handle;
		m_isWrite = isWrite;
		m_eofReached = false;
		m_collectIndex = 0;
		m_error = IODeviceOperationResult::Success;

		const Usize blockCount = std::min(MAX_ASYNC_OVERLAP_BLOCK,
			(totalSize + MaxAsyncBlockSize - 1) / MaxAsyncBlockSize);

		m_launchedCount = blockCount;
		Usize remaining = totalSize;
		U64 offset = beginOffset;

		for (Usize index = 0; index < blockCount; ++index)
		{
			BlockContext* block = BlockAt(index);
			const DWORD blockSize = static_cast<DWORD>(std::min<Usize>(MaxAsyncBlockSize, remaining));
			DWORD transferred = 0;

			if (block->Overlapped.hEvent != nullptr)
			{
				ResetEvent(block->Overlapped.hEvent);
				block->Ready = true;
			}

			block->Overlapped.Offset = static_cast<DWORD>(offset & 0xFFFFFFFFull);
			block->Overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
			block->State = OverlapBlockState::Idle;

			const BOOL result = isWrite
				? WriteFile(handle, buffer + outLaunchedSize, blockSize, &transferred, &block->Overlapped)
				: ReadFile(handle, const_cast<U8*>(buffer) + outLaunchedSize, blockSize, &transferred, &block->Overlapped);

			if (result != FALSE)
			{
				// 立即完成：该块不走HasOverlappedIoCompleted路径（此时并未返回过ERROR_IO_PENDING）
				block->State = OverlapBlockState::Finished;
			}
			else
			{
				const DWORD error = GetLastError();

				// 读到文件末尾：该块未产生结果，由CollectResult按EOF处理
				if (!isWrite && FileDeviceDetail::IsStreamEndError(error))
				{
					block->State = OverlapBlockState::Finished;
					m_eofReached = true;
					m_launchedCount = index; // 本块不计入需要收集的块
					return IODeviceOperationResult::Success;
				}

				if (error == ERROR_IO_PENDING)
				{
					block->State = OverlapBlockState::InFlight;
					block->Ready = false; // 完成后需要ResetEvent
				}
				else
				{
					// 发起失败：取消并等待其余已提交的块落地，避免留下在途IO
					block->State = OverlapBlockState::Idle;
					m_launchedCount = index;
					Cancel();

					if (m_launchedCount == 0)
						m_error = FileDeviceDetail::MapWin32Error(error);

					return FileDeviceDetail::MapWin32Error(error);
				}
			}

			outLaunchedSize += blockSize;
			remaining -= blockSize;
			offset += blockSize;
		}

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::OverlapBatch::CollectResult(HANDLE handle,
		Usize& outTransferredSize) noexcept
	{
		outTransferredSize = 0;

		for (Usize index = m_collectIndex; index < m_launchedCount; ++index)
		{
			BlockContext* block = BlockAt(index);

			if (block->State == OverlapBlockState::InFlight)
			{
				// 阶段二：取回结果，同时让内核确认该OVERLAPPED不再被引用
				DWORD transferred = 0;
				const BOOL result = GetOverlappedResult(handle, &block->Overlapped, &transferred, TRUE);

				block->State = OverlapBlockState::Finished;

				if (block->Overlapped.hEvent != nullptr)
				{
					ResetEvent(block->Overlapped.hEvent);
					block->Ready = true;
				}

				if (result == FALSE)
				{
					const DWORD error = GetLastError();

					if (!m_isWrite && FileDeviceDetail::IsStreamEndError(error))
					{
						m_eofReached = true;
					}
					else if (m_error == IODeviceOperationResult::Success)
					{
						m_error = FileDeviceDetail::MapWin32Error(error);
					}

					// 该块失败：后续块不属于连续读写区间，停止累计
					break;
				}

				outTransferredSize += transferred;
			}
			else if (block->State == OverlapBlockState::Finished && block->Overlapped.hEvent != nullptr)
			{
				// 立即完成的块：取回实际传输量（不等待）
				DWORD transferred = 0;
				if (GetOverlappedResult(handle, &block->Overlapped, &transferred, FALSE) != FALSE)
					outTransferredSize += transferred;

				if (!block->Ready)
				{
					ResetEvent(block->Overlapped.hEvent);
					block->Ready = true;
				}
			}
		}

		m_collectIndex = m_launchedCount;

		if (m_error != IODeviceOperationResult::Success)
			return m_error;

		if (m_eofReached && !m_isWrite)
			return IODeviceOperationResult::StreamEnd;

		return IODeviceOperationResult::Success;
	}

	// ------------------------------------------------------------------
	// 异步接口
	// ------------------------------------------------------------------

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::FlushBeforeAsyncOperation()
	{
		if (m_buffer.Empty())
			co_return IODeviceOperationResult::Success;

		// 小缓冲同步刷、大缓冲异步刷：避免为几KB数据付一次协程往返
		if (m_buffer.Size() <= m_syncBufferSize)
			co_return FlushInternal();

		co_return co_await AsyncFlush();
	}

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::InternalAsyncWrite(uintptr_t handleValue,
		const U8* data, Usize size, U64 beginOffset, Usize& outWrittenSize) const
	{
		outWrittenSize = 0;

		if (size == 0)
			co_return IODeviceOperationResult::Success;

		const HANDLE handle = reinterpret_cast<HANDLE>(handleValue);
		IODeviceOperationResult result = IODeviceOperationResult::Success;
		Usize written = 0;

		while (written < size)
		{
			OverlapBatch batch;

			Usize launched = 0;
			result = batch.Launch(true, handle, data + written, size - written, beginOffset + written, launched);

			if (result != IODeviceOperationResult::Success)
				break;

			// 阶段一由调度器通过IsReady驱动；恢复后进入阶段二
			co_await batch;

			Usize transferred = 0;
			result = batch.CollectResult(handle, transferred);
			written += transferred;

			if (result != IODeviceOperationResult::Success)
				break;

			if (transferred == 0)
			{
				// 写方向不应出现0字节成功，防御性终止
				result = IODeviceOperationResult::IOFailure;
				break;
			}
		}

		outWrittenSize = written;
		co_return result;
	}

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::AsyncFlush()
	{
		// m_working 由协程体置位/清位：一次性驱动到完成时也能正确恢复现场
		const bool previousWorking = std::exchange(m_working, true);
		ResourceScopeGuard workingGuard([this, previousWorking]() noexcept { m_working = previousWorking; });

		if (!IsOpen())
			co_return IODeviceOperationResult::FileNotOpen;

		if (!(m_mode & Mode::CanWrite))
			co_return IODeviceOperationResult::InvalidMode;

		if (!IsAsyncMode())
			co_return IODeviceOperationResult::InvalidMode;

		if (m_buffer.Empty())
			co_return IODeviceOperationResult::Success;

		const HANDLE handle = m_handleType == FileHandleType::Win ? m_handle.Win32Handle : nullptr;
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
			co_return IODeviceOperationResult::InvalidMode;

		const std::expected<U64, IODeviceOperationResult> posResult = TellCurrent();
		if (!posResult.has_value())
			co_return posResult.error();

		Usize written = 0;
		const IODeviceOperationResult writeResult = co_await InternalAsyncWrite(reinterpret_cast<uintptr_t>(handle),
			m_buffer.Data(), m_buffer.Size(), posResult.value(), written);

		if (writeResult != IODeviceOperationResult::Success)
			co_return writeResult;

		if (written != m_buffer.Size())
			co_return IODeviceOperationResult::IOFailure;

		if (const IODeviceOperationResult seekResult = SeekCurrent(posResult.value() + written); seekResult != IODeviceOperationResult::Success)
			co_return seekResult;

		m_buffer.Clear();
		co_return IODeviceOperationResult::Success;
	}

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::AsyncWriteFromInternal(const U8* buf,
		Usize size, bool copyToInternal)
	{
		if (buf == nullptr)
			co_return IODeviceOperationResult::InvalidArgument;

		if (size == 0)
			co_return IODeviceOperationResult::Success;

		const HANDLE handle = m_handleType == FileHandleType::Win ? m_handle.Win32Handle : nullptr;
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
			co_return IODeviceOperationResult::InvalidMode;

		// 能整体放进内部缓冲区时优先缓冲：避免为一次性大数据撑大缓冲区容量
		if (copyToInternal && m_buffer.Size() + size <= m_syncBufferSize)
		{
			if (const IODeviceOperationResult appendResult = WriteFromInternal(buf, size); appendResult != IODeviceOperationResult::Success)
				co_return appendResult;

			co_return co_await AsyncFlush();
		}

		// 1) 先把内部积压数据落盘，保证写入顺序
		if (const IODeviceOperationResult flushResult = co_await FlushBeforeAsyncOperation(); flushResult != IODeviceOperationResult::Success)
			co_return flushResult;

		const std::expected<U64, IODeviceOperationResult> posResult = TellCurrent();
		if (!posResult.has_value())
			co_return posResult.error();

		const U64 startOffset = posResult.value();

		if (copyToInternal)
		{
			// 拷进内部缓冲区后再异步写：整条链路只操作自有内存
			m_buffer.Append(buf, size);

			Usize written = 0;
			const IODeviceOperationResult writeResult = co_await InternalAsyncWrite(
				reinterpret_cast<uintptr_t>(handle), m_buffer.Data(), m_buffer.Size(), startOffset, written);

			if (writeResult != IODeviceOperationResult::Success)
				co_return writeResult;

			if (written != m_buffer.Size())
				co_return IODeviceOperationResult::IOFailure;

			if (const IODeviceOperationResult seekResult = SeekCurrent(startOffset + written); seekResult != IODeviceOperationResult::Success)
				co_return seekResult;

			m_buffer.Clear();
			co_return IODeviceOperationResult::Success;
		}

		// copyToInternal == false：直接引用调用方内存，调用方需保证其活到本任务完成
		Usize written = 0;
		const IODeviceOperationResult writeResult = co_await InternalAsyncWrite(
			reinterpret_cast<uintptr_t>(handle), buf, size, startOffset, written);

		if (writeResult != IODeviceOperationResult::Success)
			co_return writeResult;

		if (written != size)
			co_return IODeviceOperationResult::IOFailure;

		co_return SeekCurrent(startOffset + written);
	}

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::AsyncWriteFrom(const U8* buf, Usize size,
		bool copyToInternal)
	{
		if (buf == nullptr)
			co_return IODeviceOperationResult::InvalidArgument;

		if (!IsOpen())
			co_return IODeviceOperationResult::FileNotOpen;

		if (!(m_mode & Mode::CanWrite))
			co_return IODeviceOperationResult::InvalidMode;

		if (!IsAsyncMode())
			co_return IODeviceOperationResult::InvalidMode;

		const bool previousWorking = std::exchange(m_working, true);
		ResourceScopeGuard workingGuard([this, previousWorking]() noexcept { m_working = previousWorking; });

		co_return co_await AsyncWriteFromInternal(buf, size, copyToInternal);
	}

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::AsyncWriteAllFrom(const ByteArray& buf)
	{
		return AsyncWriteFrom(buf.Data(), buf.Size(), true);
	}

	CoroutineTask<IODeviceOperationResult> Win32FileDevice::AsyncWriteAllFrom(const IOutputBuffer& buf)
	{
		return AsyncWriteFrom(buf.Data(), buf.Size(), true);
	}

	CoroutineTask<std::expected<Usize, IODeviceOperationResult>> Win32FileDevice::AsyncReadToInternal(
		U8* buf, Usize size)
	{
		if (buf == nullptr)
			co_return std::unexpected(IODeviceOperationResult::InvalidArgument);

		if (size == 0)
			co_return 0;

		const HANDLE handle = m_handleType == FileHandleType::Win ? m_handle.Win32Handle : nullptr;
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
			co_return std::unexpected(IODeviceOperationResult::InvalidMode);

		// 1) 读取前先让积压数据落盘，避免读到磁盘上的旧数据
		if (const IODeviceOperationResult flushResult = co_await FlushBeforeAsyncOperation(); flushResult != IODeviceOperationResult::Success)
			co_return std::unexpected(flushResult);

		const std::expected<U64, IODeviceOperationResult> posResult = TellCurrent();
		if (!posResult.has_value())
			co_return std::unexpected(posResult.error());

		const U64 startOffset = posResult.value();

		Usize totalRead = 0;
		IODeviceOperationResult result = IODeviceOperationResult::Success;

		while (totalRead < size)
		{
			OverlapBatch batch;

			Usize launched = 0;
			result = batch.Launch(false, handle, buf + totalRead, size - totalRead, startOffset + totalRead, launched);

			if (result != IODeviceOperationResult::Success)
				break;

			// 阶段一由调度器通过IsReady驱动；恢复后进入阶段二
			co_await batch;

			Usize transferred = 0;
			const IODeviceOperationResult collectResult = batch.CollectResult(handle, transferred);
			totalRead += transferred;

			// EOF只表示可以提前结束，不是错误
			if (collectResult == IODeviceOperationResult::StreamEnd)
				break;

			if (collectResult != IODeviceOperationResult::Success)
			{
				result = collectResult;
				break;
			}

			if (transferred == 0)
				break; // 未读到数据且无错误：视作EOF
		}

		if (result != IODeviceOperationResult::Success)
			co_return std::unexpected(result);

		// 推进内核文件指针：只有协程函数可以操作位置
		if (const IODeviceOperationResult seekResult = SeekCurrent(startOffset + totalRead); seekResult != IODeviceOperationResult::Success)
			co_return std::unexpected(seekResult);

		co_return totalRead;
	}

	CoroutineTask<std::expected<Usize, IODeviceOperationResult>> Win32FileDevice::AsyncReadTo(U8* buf,
		Usize size)
	{
		if (buf == nullptr)
			co_return std::unexpected(IODeviceOperationResult::InvalidArgument);

		if (!IsOpen())
			co_return std::unexpected(IODeviceOperationResult::FileNotOpen);

		if (!(m_mode & Mode::CanRead))
			co_return std::unexpected(IODeviceOperationResult::InvalidMode);

		if (!IsAsyncMode())
			co_return std::unexpected(IODeviceOperationResult::InvalidMode);

		const bool previousWorking = std::exchange(m_working, true);
		ResourceScopeGuard workingGuard([this, previousWorking]() noexcept { m_working = previousWorking; });

		co_return co_await AsyncReadToInternal(buf, size);
	}

	CoroutineTask<std::expected<Usize, IODeviceOperationResult>> Win32FileDevice::AsyncReadAllTo(
		ByteArray& buf)
	{
		const std::expected<Usize, IODeviceOperationResult> fileSize = FileSize();
		if (!fileSize.has_value())
			co_return std::unexpected(fileSize.error());

		buf.DiscardForRead(fileSize.value());
		co_return co_await AsyncReadTo(buf.Data(), fileSize.value());
	}

	CoroutineTask<std::expected<Usize, IODeviceOperationResult>> Win32FileDevice::AsyncReadAllTo(
		IInputBuffer& buf)
	{
		const std::expected<Usize, IODeviceOperationResult> fileSize = FileSize();
		if (!fileSize.has_value())
			co_return std::unexpected(fileSize.error());

		co_return co_await AsyncReadTo(buf, fileSize.value());
	}

	IODeviceOperationResult Win32FileDevice::SeekCurrent(U64 pos) const noexcept
	{
		if (m_handleType != FileHandleType::Win)
			return IODeviceOperationResult::InvalidMode;

		LARGE_INTEGER offset{ .QuadPart = static_cast<I64>(pos) };
		if (!SetFilePointerEx(m_handle.Win32Handle, offset, nullptr, FILE_BEGIN))
			return IODeviceOperationResult::OSFailure;

		return IODeviceOperationResult::Success;
	}

	std::expected<U64, IODeviceOperationResult> Win32FileDevice::TellCurrent() const noexcept
	{
		if (m_handleType != FileHandleType::Win)
			return std::unexpected(IODeviceOperationResult::InvalidMode);

		LARGE_INTEGER zero{ .QuadPart = 0 };
		LARGE_INTEGER currentPos{ .QuadPart = 0 };

		if (!SetFilePointerEx(m_handle.Win32Handle, zero, &currentPos, FILE_CURRENT))
			return std::unexpected(IODeviceOperationResult::OSFailure);

		return static_cast<U64>(currentPos.QuadPart);
	}

	IODeviceOperationResult Win32FileDevice::Seek(U64 pos) noexcept
	{
		if (!m_open)
			return IODeviceOperationResult::FileNotOpen;

		if (m_working)
			return IODeviceOperationResult::AsyncTaskRunning;

		if (m_handleType == FileHandleType::Win)
		{
			LARGE_INTEGER offset{ .QuadPart = static_cast<I64>(pos) };
			// 指定 FILE_BEGIN 时 offset 会被视作无符号量
			if (SetFilePointerEx(m_handle.Win32Handle, offset, nullptr, FILE_BEGIN))
				return IODeviceOperationResult::Success;

			return IODeviceOperationResult::OSFailure;
		}

		if (m_handleType == FileHandleType::Px && m_handle.PxHandle != nullptr)
			return fseek(m_handle.PxHandle, static_cast<long>(pos), SEEK_SET) == 0
			? IODeviceOperationResult::Success
			: IODeviceOperationResult::OSFailure;

		return IODeviceOperationResult::FileNotOpen;
	}

	std::expected<U64, IODeviceOperationResult> Win32FileDevice::Tell() noexcept
	{
		if (!m_open)
			return std::unexpected(IODeviceOperationResult::FileNotOpen);

		if (m_working)
			return std::unexpected(IODeviceOperationResult::AsyncTaskRunning);

		if (m_handleType == FileHandleType::Win)
		{
			LARGE_INTEGER zero{ .QuadPart = 0 };
			LARGE_INTEGER currentPos{ .QuadPart = 0 };

			if (SetFilePointerEx(m_handle.Win32Handle, zero, &currentPos, FILE_CURRENT))
				return static_cast<U64>(currentPos.QuadPart);

			return std::unexpected(IODeviceOperationResult::OSFailure);
		}

		if (m_handleType == FileHandleType::Px && m_handle.PxHandle != nullptr)
		{
			const long currentPos = ftell(m_handle.PxHandle);
			if (currentPos == -1)
				return std::unexpected(IODeviceOperationResult::OSFailure);

			return static_cast<U64>(currentPos);
		}

		return std::unexpected(IODeviceOperationResult::FileNotOpen);
	}

	std::expected<Usize, IODeviceOperationResult> Win32FileDevice::FileSize() const
	{
		if (m_path.Empty())
			return std::unexpected(IODeviceOperationResult::InvalidArgument);

		return FileSize(m_path);
	}

	std::expected<Usize, IODeviceOperationResult> Win32FileDevice::FileSize(const Path& target)
	{
		if (target.Empty())
			return std::unexpected(IODeviceOperationResult::InvalidArgument);

		WIN32_FILE_ATTRIBUTE_DATA fileInfo = {};

		if (GetFileAttributesEx(target.ToString<wchar_t>().Data(), GetFileExInfoStandard, &fileInfo))
			return (static_cast<ULONGLONG>(fileInfo.nFileSizeHigh) << 32) | fileInfo.nFileSizeLow;

		switch (GetLastError())
		{
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return std::unexpected(IODeviceOperationResult::TargetNotFound);
			case ERROR_INVALID_DRIVE:
			case ERROR_INVALID_NAME:
				return std::unexpected(IODeviceOperationResult::InvalidArgument);
			case ERROR_ACCESS_DENIED:
				return std::unexpected(IODeviceOperationResult::AccessDenied);
			default:
				return std::unexpected(IODeviceOperationResult::UnknownError);
		}
	}

	bool Win32FileDevice::Exists() const
	{
		return !m_path.Empty() && Exists(m_path);
	}

	bool Win32FileDevice::Exists(const Path& path)
	{
		return !path.Empty() && GetFileAttributes(path.ToString<wchar_t>().Data()) != INVALID_FILE_ATTRIBUTES;
	}

	bool Win32FileDevice::IsReadOnly() const
	{
		return !m_path.Empty() && IsReadOnly(m_path);
	}

	bool Win32FileDevice::IsReadOnly(const Path& path)
	{
		if (path.Empty())
			return false;

		const DWORD attribute = GetFileAttributes(path.ToString<wchar_t>().Data());
		return attribute != INVALID_FILE_ATTRIBUTES && (attribute & FILE_ATTRIBUTE_READONLY) != 0;
	}

	bool Win32FileDevice::IsDirectory() const
	{
		return !m_path.Empty() && IsDirectory(m_path);
	}

	bool Win32FileDevice::IsDirectory(const Path& path)
	{
		if (path.Empty())
			return false;

		const DWORD attribute = GetFileAttributes(path.ToString<wchar_t>().Data());
		return attribute != INVALID_FILE_ATTRIBUTES && (attribute & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	bool Win32FileDevice::IsRegularFile() const
	{
		return !m_path.Empty() && IsRegularFile(m_path);
	}

	bool Win32FileDevice::IsRegularFile(const Path& path)
	{
		if (path.Empty())
			return false;

		const DWORD attribute = GetFileAttributes(path.ToString<wchar_t>().Data());
		return attribute != INVALID_FILE_ATTRIBUTES && (attribute & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	IODeviceOperationResult Win32FileDevice::Rename(StringView name)
	{
		if (name.Empty())
			return IODeviceOperationResult::InvalidArgument;

		// MoveFile 需要独占访问，先关闭当前句柄并记录模式
		const bool wasOpen = m_open;
		const ModeFlag previousMode = m_mode;

		if (wasOpen && Close() != IODeviceOperationResult::Success)
			return IODeviceOperationResult::IOFailure;

		Path newPath = m_path;
		newPath.ReplaceFilename(Path(name));

		if (!MoveFile(m_path.ToString<wchar_t>().Data(), newPath.ToString<wchar_t>().Data()))
		{
			const DWORD error = GetLastError();

			if (wasOpen)
				(void)Open(m_path, previousMode);

			switch (error)
			{
				case ERROR_ACCESS_DENIED:
					return IODeviceOperationResult::AccessDenied;
				case ERROR_FILE_NOT_FOUND:
				case ERROR_PATH_NOT_FOUND:
					return IODeviceOperationResult::TargetNotFound;
				case ERROR_SHARING_VIOLATION:
					return IODeviceOperationResult::SharingViolation;
				case ERROR_ALREADY_EXISTS:
				case ERROR_FILE_EXISTS:
					return IODeviceOperationResult::TargetAlreadyExists;
				default:
					return IODeviceOperationResult::UnknownError;
			}
		}

		m_path = std::move(newPath);

		if (wasOpen)
			return Open(m_path, previousMode);

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::Move(const Path& target, bool overwrite)
	{
		if (target.Empty())
			return IODeviceOperationResult::InvalidArgument;

		const bool wasOpen = m_open;
		const ModeFlag previousMode = m_mode;

		if (wasOpen && Close() != IODeviceOperationResult::Success)
			return IODeviceOperationResult::IOFailure;

		const DWORD flags = overwrite ? (MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) : MOVEFILE_COPY_ALLOWED;

		if (!MoveFileEx(m_path.ToString<wchar_t>().Data(), target.ToString<wchar_t>().Data(), flags))
		{
			const DWORD error = GetLastError();

			if (wasOpen)
				(void)Open(m_path, previousMode);

			switch (error)
			{
				case ERROR_ACCESS_DENIED:
					return IODeviceOperationResult::AccessDenied;
				case ERROR_FILE_NOT_FOUND:
				case ERROR_PATH_NOT_FOUND:
					return IODeviceOperationResult::TargetNotFound;
				case ERROR_SHARING_VIOLATION:
					return IODeviceOperationResult::SharingViolation;
				case ERROR_ALREADY_EXISTS:
				case ERROR_FILE_EXISTS:
					return IODeviceOperationResult::TargetAlreadyExists;
				default:
					return IODeviceOperationResult::UnknownError;
			}
		}

		m_path = target;

		if (wasOpen)
			return Open(m_path, previousMode);

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::Copy(const Path& target, bool overwrite) const
	{
		if (target.Empty())
			return IODeviceOperationResult::InvalidArgument;

		if (CopyFile(m_path.ToString<wchar_t>().Data(), target.ToString<wchar_t>().Data(), overwrite ? FALSE : TRUE))
			return IODeviceOperationResult::Success;

		switch (GetLastError())
		{
			case ERROR_ACCESS_DENIED:
				return IODeviceOperationResult::AccessDenied;
			case ERROR_FILE_NOT_FOUND:
			case ERROR_PATH_NOT_FOUND:
				return IODeviceOperationResult::TargetNotFound;
			case ERROR_SHARING_VIOLATION:
				return IODeviceOperationResult::SharingViolation;
			case ERROR_ALREADY_EXISTS:
			case ERROR_FILE_EXISTS:
				return IODeviceOperationResult::TargetAlreadyExists;
			default:
				return IODeviceOperationResult::UnknownError;
		}
	}

	IODeviceOperationResult Win32FileDevice::MatchSeek(StringView data)
	{
		return InternalMatchSeek(reinterpret_cast<const U8*>(data.Data()), data.Size());
	}

	IODeviceOperationResult Win32FileDevice::MatchSeek(const ByteArray& data)
	{
		return InternalMatchSeek(data.Data(), data.Size());
	}

	IODeviceOperationResult Win32FileDevice::Remove()
	{
		if (m_path.Empty())
			return IODeviceOperationResult::InvalidArgument;

		if (m_open && Close() != IODeviceOperationResult::Success)
			return IODeviceOperationResult::IOFailure;

		if (!FileDeviceDetail::RemoveRecursive(m_path))
			return IODeviceOperationResult::IOFailure;

		m_path.Clear();
		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::Remove(const Path& target)
	{
		if (target.Empty())
			return IODeviceOperationResult::InvalidArgument;

		if (!FileDeviceDetail::RemoveRecursive(target))
			return IODeviceOperationResult::IOFailure;

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::MoveToTrash()
	{
		if (m_path.Empty())
			return IODeviceOperationResult::InvalidArgument;

		if (m_open && Close() != IODeviceOperationResult::Success)
			return IODeviceOperationResult::IOFailure;

		return MoveToTrash(m_path);
	}

	IODeviceOperationResult Win32FileDevice::MoveToTrash(const Path& target)
	{
		if (target.Empty())
			return IODeviceOperationResult::InvalidArgument;

		if (!Exists(target))
			return IODeviceOperationResult::TargetNotFound;

		// SHFileOperationW 要求以双 \0 结尾的路径列表
		std::wstring from = target.ToAbsolutePathWString().ConvertToStdString<wchar_t>();
		from.push_back(L'\0');
		from.push_back(L'\0');

		SHFILEOPSTRUCTW operation = {};
		operation.wFunc = FO_DELETE;
		operation.pFrom = from.c_str();
		operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

		const int result = SHFileOperationW(&operation);

		if (result != 0)
		{
			switch (result)
			{
				case ERROR_ACCESS_DENIED:
					return IODeviceOperationResult::AccessDenied;
				case ERROR_FILE_NOT_FOUND:
				case ERROR_PATH_NOT_FOUND:
					return IODeviceOperationResult::TargetNotFound;
				default:
					return IODeviceOperationResult::IOFailure;
			}
		}

		if (operation.fAnyOperationsAborted)
			return IODeviceOperationResult::IOFailure;

		return IODeviceOperationResult::Success;
	}

	IODeviceOperationResult Win32FileDevice::ResetFileSize(U64 size)
	{
		if (!m_open)
		{
			if (m_path.Empty())
				return IODeviceOperationResult::FileNotOpen;

			// 尚未打开：用临时设备以读写模式打开后调整大小
			return ResetFileSize(m_path, size);
		}

		// 调整大小前先冲刷缓冲并落盘，避免与在途写竞争
		if (!m_buffer.Empty())
			if (IODeviceOperationResult flushResult = Flush(); flushResult != IODeviceOperationResult::Success)
				return flushResult;

		std::expected<U64, IODeviceOperationResult> posResult = Tell();
		if (!posResult.has_value())
			return posResult.error();

		const U64 currentPos = posResult.value();

		if (m_handleType == FileHandleType::Win)
		{
			LARGE_INTEGER offset{ .QuadPart = static_cast<I64>(size) };
			if (!SetFilePointerEx(m_handle.Win32Handle, offset, nullptr, FILE_BEGIN) || !SetEndOfFile(m_handle.Win32Handle))
			{
				(void)Seek(currentPos);
				return IODeviceOperationResult::IOFailure;
			}

			// 原位置若已被截断则回退到文件末尾
			return Seek(std::min(currentPos, size));
		}

		if (m_handleType == FileHandleType::Px && m_handle.PxHandle != nullptr)
		{
			if (_chsize_s(_fileno(m_handle.PxHandle), static_cast<long long>(size)) != 0)
			{
				(void)Seek(currentPos);
				return IODeviceOperationResult::IOFailure;
			}

			return Seek(std::min(currentPos, size));
		}

		return IODeviceOperationResult::FileNotOpen;
	}

	IODeviceOperationResult Win32FileDevice::ResetFileSize(const Path& target, U64 size)
	{
		if (target.Empty())
			return IODeviceOperationResult::InvalidArgument;

		Win32FileDevice temp;
		if (IODeviceOperationResult openResult = temp.Open(target, Mode::CanWriteAndRead); openResult != IODeviceOperationResult::Success)
			return openResult;

		return temp.ResetFileSize(size);
	}

	IODeviceOperationResult Win32FileDevice::InternalSyncWrite(const U8* data, Usize reqSize) const noexcept
	{
		// 防御应该由前台处理

		if (m_handleType == FileHandleType::Win)
		{
			// FILE_FLAG_OVERLAPPED 句柄上的读写必须携带 OVERLAPPED，否则行为未定义；
			// 这里在栈上构造 OVERLAPPED，并用设备自身的事件 m_overlapEvent 等待完成
			const bool overlappedMode = m_mode.Test(Mode::EnableAsyncOperation);

			if (overlappedMode && (m_overlapEvent == nullptr || m_overlapEvent == INVALID_HANDLE_VALUE))
				return IODeviceOperationResult::OSFailure;

			LARGE_INTEGER zero{ .QuadPart = 0 };
			LARGE_INTEGER currentPos{ .QuadPart = 0 };

			// 重叠操作不会推进文件指针，因此手动维护本次写入的起始偏移
			U64 writeOffset = 0;

			if (overlappedMode)
			{
				if (!SetFilePointerEx(m_handle.Win32Handle, zero, &currentPos, FILE_CURRENT))
					return IODeviceOperationResult::IOFailure;

				writeOffset = static_cast<U64>(currentPos.QuadPart);
			}

			Usize totalWrittenSize = 0;

			while (totalWrittenSize < reqSize)
			{
				const DWORD currentBlockSize = static_cast<DWORD>(std::min<Usize>(MaxAsyncBlockSize, reqSize - totalWrittenSize));
				DWORD currentWrittenSize = 0;

				OVERLAPPED overlapped = {};
				OVERLAPPED* overlappedPtr = nullptr;

				if (overlappedMode)
				{
					overlapped.hEvent = static_cast<HANDLE>(m_overlapEvent);
					overlapped.Offset = static_cast<DWORD>(writeOffset & 0xFFFFFFFFull);
					overlapped.OffsetHigh = static_cast<DWORD>(writeOffset >> 32);

					// 事件为手动重置，复用前必须复位
					ResetEvent(overlapped.hEvent);
					overlappedPtr = &overlapped;
				}

				const BOOL result = WriteFile(m_handle.Win32Handle, data + totalWrittenSize, currentBlockSize,
					&currentWrittenSize, overlappedPtr);

				if (result == FALSE)
				{
					const DWORD error = GetLastError();

					if (overlappedMode && error == ERROR_IO_PENDING)
					{
						// 已提交异步写入：等待事件并取回结果
						if (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_FAILED)
							return IODeviceOperationResult::IOFailure;

						if (GetOverlappedResult(m_handle.Win32Handle, &overlapped, &currentWrittenSize, FALSE) == FALSE)
							return GetLastError() == ERROR_DISK_FULL
							? IODeviceOperationResult::BufferFull
							: IODeviceOperationResult::IOFailure;
					}
					else if (totalWrittenSize == 0)
					{
						switch (error)
						{
							case ERROR_DISK_FULL:
								return IODeviceOperationResult::BufferFull;
							default:
								return IODeviceOperationResult::IOFailure;
						}
					}
					else
					{
						// 已写入部分数据，无法继续，按 IO 失败结束
						break;
					}
				}

				if (overlappedMode)
					ResetEvent(overlapped.hEvent);

				if (currentWrittenSize == 0)
					break;

				totalWrittenSize += currentWrittenSize;
				writeOffset += currentWrittenSize;
			}

			if (overlappedMode)
			{
				// 将文件指针推进到本次写入的结束位置，保证 Tell() 与后续同步操作一致
				LARGE_INTEGER finalPos{ .QuadPart = static_cast<I64>(writeOffset) };
				if (!SetFilePointerEx(m_handle.Win32Handle, finalPos, nullptr, FILE_BEGIN))
					return IODeviceOperationResult::IOFailure;
			}

			return totalWrittenSize == reqSize ? IODeviceOperationResult::Success : IODeviceOperationResult::IOFailure;
		}

		if (m_handleType != FileHandleType::Px || m_handle.PxHandle == nullptr)
			return IODeviceOperationResult::FileNotOpen;

		clearerr(m_handle.PxHandle);
		if ((fwrite(data, reqSize, 1, m_handle.PxHandle) == 1) && (fflush(m_handle.PxHandle) == 0))
			return IODeviceOperationResult::Success;

		clearerr(m_handle.PxHandle); // 清除错误标志，允许后续操作
		switch (errno)
		{
			case EBADF:
				return IODeviceOperationResult::InvalidMode;
			case ENOSPC:
				return IODeviceOperationResult::BufferFull;
			default:
				return IODeviceOperationResult::IOFailure;
		}
	}

	std::expected<Usize, IODeviceOperationResult> Win32FileDevice::InternalSyncRead(U8* buffer,
		Usize reqSize) const noexcept
	{
		// 防御应该由前台处理

		if (m_handleType == FileHandleType::Win)
		{
			// 与 InternalSyncWrite 对称：重叠句柄必须携带 OVERLAPPED，并用 m_overlapEvent 等待完成
			const bool overlappedMode = m_mode.Test(Mode::EnableAsyncOperation);

			if (overlappedMode && (m_overlapEvent == nullptr || m_overlapEvent == INVALID_HANDLE_VALUE))
				return std::unexpected(IODeviceOperationResult::OSFailure);

			LARGE_INTEGER zero{ .QuadPart = 0 };
			LARGE_INTEGER currentPos{ .QuadPart = 0 };

			U64 readOffset = 0;

			if (overlappedMode)
			{
				if (!SetFilePointerEx(m_handle.Win32Handle, zero, &currentPos, FILE_CURRENT))
					return std::unexpected(IODeviceOperationResult::IOFailure);

				readOffset = static_cast<U64>(currentPos.QuadPart);
			}

			Usize totalReadSize = 0;

			while (totalReadSize < reqSize)
			{
				const DWORD blockSize = static_cast<DWORD>(std::min<Usize>(MaxAsyncBlockSize, reqSize - totalReadSize));
				DWORD bytesRead = 0;

				OVERLAPPED overlapped = {};
				OVERLAPPED* overlappedPtr = nullptr;

				if (overlappedMode)
				{
					overlapped.hEvent = static_cast<HANDLE>(m_overlapEvent);
					overlapped.Offset = static_cast<DWORD>(readOffset & 0xFFFFFFFFull);
					overlapped.OffsetHigh = static_cast<DWORD>(readOffset >> 32);

					// 事件为手动重置，复用前必须复位
					ResetEvent(overlapped.hEvent);
					overlappedPtr = &overlapped;
				}

				const BOOL result = ReadFile(m_handle.Win32Handle, buffer + totalReadSize, blockSize,
					&bytesRead, overlappedPtr);

				if (result == FALSE)
				{
					const DWORD error = GetLastError();

					// 重叠句柄读到文件末尾时返回 ERROR_HANDLE_EOF，而非 0 字节成功
					if (overlappedMode && error == ERROR_HANDLE_EOF)
						break;

					if (overlappedMode && error == ERROR_IO_PENDING)
					{
						// 已提交异步读取：等待事件并取回结果
						if (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_FAILED)
							return std::unexpected(IODeviceOperationResult::IOFailure);

						if (GetOverlappedResult(m_handle.Win32Handle, &overlapped, &bytesRead, FALSE) == FALSE)
						{
							// 异步提交后才到达文件末尾的情况
							if (GetLastError() == ERROR_HANDLE_EOF)
								break;

							return std::unexpected(IODeviceOperationResult::IOFailure);
						}
					}
					else if (totalReadSize == 0)
					{
						return std::unexpected(IODeviceOperationResult::IOFailure);
					}
					else
					{
						// 已读到部分数据后失败，按已读到的大小返回
						break;
					}
				}

				if (overlappedMode)
					ResetEvent(overlapped.hEvent);

				// 读到 0 字节：源文件 EOF，正常结束短读
				if (bytesRead == 0)
					break;

				totalReadSize += bytesRead;
				readOffset += bytesRead;
			}

			if (overlappedMode)
			{
				// 将文件指针推进到本次读取的结束位置，保证 Tell() 与后续同步操作一致
				LARGE_INTEGER finalPos{ .QuadPart = static_cast<I64>(readOffset) };
				if (!SetFilePointerEx(m_handle.Win32Handle, finalPos, nullptr, FILE_BEGIN))
					return std::unexpected(IODeviceOperationResult::IOFailure);
			}

			return totalReadSize;
		}

		if (m_handleType != FileHandleType::Px || m_handle.PxHandle == nullptr)
			return std::unexpected(IODeviceOperationResult::FileNotOpen);

		Usize totalRead = 0;

		while (totalRead < reqSize)
		{
			Usize blockSize = reqSize - totalRead;
			blockSize = std::min(blockSize, MaxAsyncBlockSize);

			clearerr(m_handle.PxHandle);
			errno = 0;
			Usize bytesRead = fread(buffer + totalRead, 1, blockSize, m_handle.PxHandle);
			totalRead += bytesRead;

			// 检查是否应该退出
			// 读少了，检查原因
			if (bytesRead < blockSize)
			{
				if (feof(m_handle.PxHandle))
					break;
				if (ferror(m_handle.PxHandle))
				{
					// 错误：如果是第一次读就失败，返回0；否则返回部分
					clearerr(m_handle.PxHandle);
					if (totalRead == 0)
					{
						switch (errno)
						{
							case EBADF:
								return std::unexpected(IODeviceOperationResult::InvalidMode);
							default:
								return std::unexpected(IODeviceOperationResult::IOFailure);
						}
					}
					break;
				}

				// std::unreachable();
				DEBUG_ALWAYS_REPORT("Unreachable section");
			}
		}

		return totalRead;
	}

	IODeviceOperationResult Win32FileDevice::InternalMatchSeek(const U8* data, Usize size)
	{
		if (!m_open)
			return IODeviceOperationResult::FileNotOpen;

		if (m_working)
			return IODeviceOperationResult::AsyncTaskRunning;

		if (!(m_mode & Mode::CanRead))
			return IODeviceOperationResult::InvalidMode;

		// 搜索属于读取操作，先确保已缓冲数据落盘，避免搜索到磁盘上的旧数据
		if (!m_buffer.Empty())
			if (IODeviceOperationResult flushResult = Flush(); flushResult != IODeviceOperationResult::Success)
				return flushResult;

		// 空匹配串视作在当前指针处命中
		if (size == 0)
			return IODeviceOperationResult::Success;

		std::expected<U64, IODeviceOperationResult> posResult = Tell();
		if (!posResult.has_value())
			return posResult.error();
		const U64 startPos = posResult.value();

		std::expected<Usize, IODeviceOperationResult> sizeResult = FileSize();
		if (!sizeResult.has_value())
			return sizeResult.error();
		const U64 fileSize = sizeResult.value();

		if (startPos > fileSize || (fileSize - startPos) < size)
			return IODeviceOperationResult::TargetNotFound;

		// 使用固定大小块读取，内存占用与文件大小无关：
		// 多出的 size - 1 字节用于缓存跨块边界可能存在的匹配前缀，避免回读文件
		constexpr Usize SearchBlockSize = 64 * 1024;
		ByteArray window(SearchBlockSize + size - 1);

		const U8* pattern = data;
		U64 windowStart = startPos;      // window[0] 对应的绝对文件偏移
		Usize carry = 0;                 // window[0 .. carry) 为上一块遗留的尾部数据
		U64 remaining = fileSize - startPos;

		while (remaining != 0)
		{
			// 总读取量不超过 SearchBlockSize，因此 window 一定能容纳 carry + 本次读取
			const Usize want = static_cast<Usize>(std::min<U64>(remaining, SearchBlockSize));

			std::expected<Usize, IODeviceOperationResult> readResult = InternalSyncRead(window.Data() + carry, want);
			if (!readResult.has_value())
				return readResult.error();

			const Usize readSize = readResult.value();
			if (readSize == 0)
				break;

			const Usize available = carry + readSize;

			// 仅在能完整容纳匹配串时查找，跨块的部分由 carry 交由下一轮处理
			if (available >= size)
			{
				const U8* haystack = window.Data();
				const U8* cursor = haystack;
				const U8* searchEnd = haystack + (available - size + 1);
				const U8 firstByte = pattern[0];

				// 用 memchr 快速定位首字节，再用 memcmp 校验完整匹配
				while (cursor < searchEnd)
				{
					cursor = static_cast<const U8*>(std::memchr(cursor, firstByte,
																static_cast<Usize>(searchEnd - cursor)));
					if (cursor == nullptr)
						break;

					if (size == 1 || std::memcmp(cursor + 1, pattern + 1, size - 1) == 0)
					{
						const U64 matchPos = windowStart + static_cast<U64>(cursor - haystack);
						if (IODeviceOperationResult seekResult = Seek(matchPos); seekResult != IODeviceOperationResult::Success)
							return seekResult;

						return IODeviceOperationResult::Success;
					}

					++cursor;
				}
			}

			// 保留尾部 size - 1 字节作为下一块的前缀
			const Usize keep = available < size ? available : (size - 1);
			if (keep != 0)
				std::memmove(window.Data(), window.Data() + available - keep, keep);

			windowStart += available - keep;
			carry = keep;
			remaining -= readSize;
		}

		// 未找到匹配，恢复查找前的指针位置
		(void)Seek(startPos);
		return IODeviceOperationResult::TargetNotFound;
	}
}
