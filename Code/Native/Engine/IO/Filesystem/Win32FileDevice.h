// File /Native/Engine/IO/Win32FileDevice.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../../Coroutine/CoroutineTask.hpp"
#include "../../Coroutine/ICoroutineInstruction.hpp"
#include "../../OS/Windows/Windows.h"
#include "../../Utils/ByteArray.hpp"
#include "../../Utils/Flag.hpp"
#include "../IInputBuffer.h"
#include "../IOutputBuffer.h"
#include "../Path.h"
#include <array>
#include <expected>
#include <utility>

#include "../IIODevice.hpp"

namespace PenEngine
{
	class Win32FileDevice : public IBasicIODevice
	{
	public:
		constexpr static U64 DefaultSyncBufferSize = 4 * 1024ull; // 4KB
		constexpr static U64 AsyncThreshold = 1024ull * 1024ull; // 1MB
		constexpr static U64 MaxAsyncBlockSize = 32 * 1024ull * 1024ull; // 32MB

		enum class Mode : U16
		{
			None = 0,

			CanWrite = BIT_ENUM(1), // 指示可以向文件写入数据
			CanRead = BIT_ENUM(2), // 指示可以从文件读取数据

			CanWriteAndRead = CanWrite | CanRead,

			Text = BIT_ENUM(3), // 指示数据是文本数据
			Binary = BIT_ENUM(4), // 指示数据是二进制数据

			Append = BIT_ENUM(5), // 指示在文件尾添加数据

			NewOnly = BIT_ENUM(6), // 指示必须新建文件
			ExistingOnly = BIT_ENUM(7), // 指示文件必须存在
			IgnoreIsExist = NewOnly | ExistingOnly, // 指示忽略文件存在性，如果不存在则新建

			EnableAsyncOperation = BIT_ENUM(8), // 指示启用异步读写

			Truncate = BIT_ENUM(9), // 指示是否需要截断文件从0开始写入
			DontUseBuffer = BIT_ENUM(10), // 指示是否不需要使用Device内部的缓冲区

			DontClosePosixHandle = BIT_ENUM(11) // 指示在Close()时是否需要关闭FILE*句柄，如果指定则只会刷新缓冲区，如果不是使用FILE*方式打开文件，则该枚举无效
		};

		DECL_ENUM_FLAG_FRIEND_TYPE(Mode);

		using HANDLE = void*;
		using DWORD = unsigned long;

		PENFRAMEWORK_IODEVICE_OVERLOADING;

		Win32FileDevice() noexcept = default;

		IODeviceOperationResult Open(const Path& path, ModeFlag mode);
		IODeviceOperationResult Open(FILE* handle, ModeFlag mode);

		IODeviceOperationResult Close() noexcept;

		IODeviceOperationResult Seek(U64 pos) noexcept;
		std::expected<U64, IODeviceOperationResult> Tell() noexcept;

		virtual IODeviceOperationResult WriteFrom(const U8* buf, Usize size) override;


		/// @brief 异步写入数据
		/// @param buf 数据指针
		/// @param size 需要写入的字节数
		/// @param copyToInternal 是否先把外部数据拷贝进设备内部缓冲区。
		///        为 true 时数据由设备持有，调用方在任务完成前可以立即释放/复用 buf；
		///        为 false 时设备直接引用 buf，调用方必须保证该内存在对应 CoroutineTask
		///        被驱动至完成之前一直有效（最多 MAX_ASYNC_OVERLAP_BLOCK 块同时在途）。
		virtual CoroutineTask<IODeviceOperationResult> AsyncWriteFrom(const U8* buf, Usize size, bool copyToInternal = true) override;

		CoroutineTask<IODeviceOperationResult> AsyncWriteAllFrom(const ByteArray& buf);
		CoroutineTask<IODeviceOperationResult> AsyncWriteAllFrom(const IOutputBuffer& buf);

		/// @brief 异步将内部缓冲区中积压的数据落盘
		CoroutineTask<IODeviceOperationResult> AsyncFlush();

		virtual std::expected<Usize, IODeviceOperationResult> ReadTo(U8* buf, Usize size) override;

		std::expected<Usize, IODeviceOperationResult> ReadAllTo(IInputBuffer& buf);
		std::expected<Usize, IODeviceOperationResult> ReadAllTo(ByteArray& buf);

		virtual CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadTo(U8* buf, Usize size) override;

		CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadAllTo(IInputBuffer& buf);
		CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadAllTo(ByteArray& buf);

		IODeviceOperationResult Flush() noexcept;
		[[nodiscard]] bool IsOpen() const noexcept;

		[[nodiscard]] bool Exists() const;
		[[nodiscard]] static bool Exists(const Path& path);
		[[nodiscard]] bool IsReadOnly() const;
		[[nodiscard]] static bool IsReadOnly(const Path& path);
		[[nodiscard]] bool IsDirectory() const;
		[[nodiscard]] static bool IsDirectory(const Path& path);
		[[nodiscard]] bool IsRegularFile() const;
		[[nodiscard]] static bool IsRegularFile(const Path& path);

		std::expected<Usize, IODeviceOperationResult> FileSize() const;
		static std::expected<Usize, IODeviceOperationResult> FileSize(const Path& target);

		IODeviceOperationResult ResetFileSize(U64 size);
		static IODeviceOperationResult ResetFileSize(const Path& target, U64 size);

		IODeviceOperationResult Rename(StringView name);
		IODeviceOperationResult Move(const Path& target, bool overwrite = false);
		IODeviceOperationResult Copy(const Path& target, bool overwrite = false) const;

		// 从当前指针位置起查找数据串，命中后将指针定位到匹配头部；未命中则恢复原位置
		IODeviceOperationResult MatchSeek(StringView data);
		IODeviceOperationResult MatchSeek(const ByteArray& data);

		IODeviceOperationResult Remove();
		static IODeviceOperationResult Remove(const Path& target);

		IODeviceOperationResult MoveToTrash();
		static IODeviceOperationResult MoveToTrash(const Path& target);

		void SetSyncBufferSize(Usize bufSize) noexcept;
		[[nodiscard]] Usize GetSyncBufferSize() const noexcept;
	private:
		union FileHandle
		{
			HANDLE Win32Handle;
			FILE* PxHandle;
		};

		enum class FileHandleType : U8
		{
			Invalid,
			Px,
			Win
		};

		/// @brief 重叠批次中单个块的状态
		enum class OverlapBlockState : U8
		{
			Idle,     // 未使用
			InFlight, // 已提交且由内核持有（对应ERROR_IO_PENDING）
			Finished  // 已完成（同步立即完成，或已被IsReady检测到完成）
		};

		/// @brief 单批次内最多同时在途的重叠块数量
		constexpr static Usize MAX_ASYNC_OVERLAP_BLOCK = 4;

		/// @brief 一批并发重叠IO
		///
		/// 生命周期完全由协程帧托管：批次对象、OVERLAPPED数组与每块的事件都位于等待它的协程帧内。
		/// 两个阶段：
		///   阶段一（IsReady，由调度器驱动）：只调用HasOverlappedIoCompleted判断是否全部完成，
		///          不消费任何结果；
		///   阶段二（CollectResult，由协程体在恢复后调用）：对每个已完成块调用GetOverlappedResult
		///          取回传输字节数与错误码。
		///
		/// @note 事件在首次Launch时惰性创建（每批次一组，随批次销毁），
		///       因此不在设备上持久占用内核对象。
		class OverlapBatch final : public ICoroutineInstruction
		{
		public:
			OverlapBatch() noexcept = default;

			OverlapBatch(const OverlapBatch&) = delete;
			OverlapBatch& operator=(const OverlapBatch&) = delete;

			~OverlapBatch() noexcept override;

			/// @brief 发起下一批块（至多MAX_ASYNC_OVERLAP_BLOCK个）
			/// @param isWrite true为写，false为读
			/// @param handle 目标文件句柄
			/// @param buffer 数据基址（写：源；读：目标）
			/// @param totalSize buffer中剩余可用字节数
			/// @param beginOffset 本批第一个块对应的文件偏移
			/// @param outLaunchedSize 本批实际发起的字节数
			/// @return 发起结果；IOFailure表示某块发起失败，其余块已被取消并等待落地
			[[nodiscard]] IODeviceOperationResult Launch(bool isWrite, HANDLE handle, const U8* buffer,
				Usize totalSize, U64 beginOffset, Usize& outLaunchedSize) noexcept;

			/// @brief 阶段二：收集本批结果
			/// @param handle 目标文件句柄
			/// @param outTransferredSize 本批实际完成传输的字节数
			/// @return 首个错误；全部成功时返回Success
			[[nodiscard]] IODeviceOperationResult CollectResult(HANDLE handle, Usize& outTransferredSize) noexcept;

			/// @brief 阶段一：是否所有块都已完成（不消费结果）
			[[nodiscard]] virtual bool IsReady() noexcept override;

			/// @brief 取消所有在途块并等待其落地，保证析构不再有内核写入本对象内存
			virtual bool Cancel() override;

			[[nodiscard]] bool HasInFlightBlock() const noexcept;
		private:
			/// @brief 单块上下文：持有OVERLAPPED与启动信息
			struct BlockContext
			{
				OVERLAPPED Overlapped = {};
				OverlapBlockState State = OverlapBlockState::Idle;
				bool Ready = true; // 事件是否处于已复位状态

				BlockContext() noexcept = default;

				BlockContext(const BlockContext&) = delete;
				BlockContext& operator=(const BlockContext&) = delete;
			};

			[[nodiscard]] BlockContext* BlockAt(Usize index) noexcept;
			[[nodiscard]] const BlockContext* BlockAt(Usize index) const noexcept;

			[[nodiscard]] bool AcquireBlockEvents() noexcept;
			void ReleaseBlockEvents() noexcept;

			BlockContext m_blocks[MAX_ASYNC_OVERLAP_BLOCK];
			HANDLE m_targetHandle = nullptr;
			Usize m_launchedCount = 0;
			Usize m_collectIndex = 0;
			bool m_isWrite = true;
			bool m_eofReached = false;
			IODeviceOperationResult m_error = IODeviceOperationResult::Success;
		};

		IODeviceOperationResult InternalSyncWrite(const U8* data, Usize reqSize) const noexcept;
		std::expected<Usize, IODeviceOperationResult> InternalSyncRead(U8* buffer, Usize reqSize) const noexcept;
		IODeviceOperationResult InternalMatchSeek(const U8* data,Usize size);

		/// @brief 不检查m_working的内部版本：供已持有工作权的协程体调用，避免被自身的工作标记拒绝
		IODeviceOperationResult FlushInternal() noexcept;
		IODeviceOperationResult WriteFromInternal(const U8* buf, Usize size);

		/// @brief 单批次异步写：以重叠批次执行一段数据，返回本段写入结果
		CoroutineTask<IODeviceOperationResult> InternalAsyncWrite(uintptr_t handleValue, const U8* data, Usize size,
			U64 beginOffset, Usize& outWrittenSize) const;

		/// @brief 重叠批次循环的公共部件（不含参数与工作权校验）
		CoroutineTask<IODeviceOperationResult> AsyncWriteFromInternal(const U8* buf, Usize size, bool copyToInternal);
		CoroutineTask<std::expected<Usize, IODeviceOperationResult>> AsyncReadToInternal(U8* buf, Usize size);

		/// @brief 在需要时先让内部缓冲区落盘：小缓冲同步刷，大缓冲异步刷
		CoroutineTask<IODeviceOperationResult> FlushBeforeAsyncOperation();

		/// @brief 设置当前逻辑文件位置（重叠IO不会自动推进内核文件指针）
		IODeviceOperationResult SeekCurrent(U64 pos) const noexcept;

		/// @brief 获取当前逻辑文件位置
		std::expected<U64, IODeviceOperationResult> TellCurrent() const noexcept;

		/// @brief 是否启用了异步（重叠）模式
		[[nodiscard]] bool IsAsyncMode() const noexcept
		{
			return m_mode.Test(Mode::EnableAsyncOperation);
		}

		Path m_path;
		ByteArray m_buffer;
		FileHandle m_handle;
		void* m_overlapEvent = nullptr;
		Usize m_syncBufferSize = DefaultSyncBufferSize;
		ModeFlag m_mode;
		FileHandleType m_handleType = FileHandleType::Invalid;
		bool m_open = false;

		bool m_working = false;
	};
}
