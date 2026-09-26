# PenMemory

一个基于 [google/tcmalloc](https://github.com/google/tcmalloc) 源码结构实现的**三层简化版 tcmalloc**：

```
Allocate/Deallocate
   │
   ├── size <= 256 KiB ──► ThreadCache ──► CentralCache ──► PageCache
   │                       (per-thread)    (per-size-class)  (span)
   └── size >  256 KiB ────────────────────────────────────► PageCache
```

* **per-thread 分配**：每个线程一份 `ThreadCache`，快路径只是线程本地链表的一次 pop/push，
  没有原子操作、没有锁、没有系统调用。
* 只用 C++23（`std::has_single_bit` / `bit_width` 风格、`thread_local`、`inline` 变量、`if constexpr`），
  MSVC `/std:c++latest` 与 GCC/Clang `-std=c++23` 均可编译。
* 系统内存：Windows 用 `VirtualAlloc / VirtualFree / MEM_COMMIT / MEM_DECOMMIT`，
  Linux/macOS 用 `mmap / munmap / madvise`。

---

## 0. 在 PenFramework 中的位置

* 本模块位于 `PenFramework/Code/Native/Memory`，由根 `CMakeLists.txt` 通过 `add_subdirectory`
  构建成 `PenMemory.dll`；主程序通过导入库链接它，DLL 与 exe 输出在同一个构建目录。
* 对外接口是纯 C 的 `extern "C"` ABI，声明在 `Interface.h`，实现就是 `Interface.cpp`：
  只有 4 个分配入口 + 统计/自省函数，不暴露任何 C++ 类型，因此也可以直接作为 P/Invoke 表面。
* C++ 侧的封装在 `Code/Native/Engine/Memory/Memory.hpp`（`PenEngine::Memory`）：提供
  `Memory::Allocate<T>(count)` 这类按 `sizeof/alignof` 推导的模板；`alignof(T)` 大于分配器
  基础对齐时自动改走 `AlignedAllocate`。
* 全局 `operator new/delete` 接管在 `Code/Native/Engine/Memory/MemoryOperator.cpp`，
  只替换主程序自己的 new/delete（DLL 边界外的分配不受影响）。
* 测试：根 CMake 的统一选项 `PENFRAMEWORK_BUILD_TESTS`（默认 OFF，约定见 `Docs/Testing.md`）
  打开后可用 ctest 运行；`build.ps1` 仍可独立编译并运行测试。
* 文档中出现的旧接口名已一并更新，例如 `CentralFreeList::length()` → `CachedObjectCount()`、
  `os::PageSize()` → `os::GetSystemPageSize()`。

## 1. 文件结构

```
Common.hpp          配置常量、Length/PageId/Range、尺寸类表、SizeMap、SpinLock、断言
OSMemory.h/.cpp     OS 内存原语 + 元数据 arena（不递归进入分配器）
Radix.h/.cpp        三级基数树 RadixTree + PageMap（page → Span*）
PageCache.h/.cpp    Span 定义 + 页堆：按长度分桶、邻居合并、归还 OS
CentralCache.h/.cpp 每个 size class 的中心空闲链表（批量搬运 + span 生命周期）
ThreadCache.h/.cpp  线程本地缓存（FreeList 数组、慢启动、Scavenge、全局预算）
Interface.h/.cpp    对外 C ABI（`extern "C"`）的实现：4 个入口 + 统计/自检
Globals.hpp         全局单例 Globals（等价 tc_globals，内部 C++ 状态，不进 C ABI）
Test/UnitTest/      6 个测试程序（MiniTest 框架）
Test/Benchmark/     1 个基准程序
build.ps1           Windows/MSVC 一键编译 + 跑测试
CMakeLists.txt      PenMemory.dll 的构建（开 PENFRAMEWORK_BUILD_TESTS 时带测试）
DESIGN.md           源码理解汇报 + 设计取舍
```

依赖方向（无环）：

```
Interface.cpp (C ABI) → ThreadCache → CentralCache → PageCache → Radix → OSMemory → Common.hpp
```

> 相对推荐结构多了一个 `Common.hpp`（公共类型与尺寸类表）；`Span` 放在 `PageCache.h`
> （span 就是页堆的分配单位）；全局单例 `Globals`（等价 `tc_globals`）放在 `Globals.hpp`
> （内部 C++ 状态，不进 C ABI）。

## 2. 对外接口

```cpp
// Interface.h —— 纯 C 的 extern "C" ABI（PenMemory.dll 的导出面）
extern "C" {
void* Allocate(size_t size) noexcept;                          // 失败返回 nullptr
void  Deallocate(void* ptr) noexcept;                          // 不需要 size
void* AlignedAllocate(size_t alignment, size_t size) noexcept;  // alignment 为 2 的幂
void  AlignedDeallocate(void* ptr, size_t alignment) noexcept;

// 附带的自省接口（测试用）
size_t GetAllocatedSize(const void* ptr);
size_t GetEstimatedAllocatedSize(size_t size);
typedef struct PenMemoryStats { ... } PenMemoryStats;
void  GetStats(PenMemoryStats* stats);
size_t ReleaseFreeMemory();
uint8_t VerifyHeap(const char** error);   // 校验三层全部不变量
}
```

`Deallocate` 与 `free()` 一样**不需要 size 参数**：指针 → 页号 →（基数树）→ `Span` → size class。
size class 为 0 表示这是页级（大对象）分配，或该 span 已回到页堆（据此检测重复释放）。

## 3. 编译与运行

### 在 PenFramework 内（CMake / DLL）

根 `CMakeLists.txt` 通过 `add_subdirectory(Code/Native/Memory)` 生成 `PenMemory.dll` 与导入库，
主程序链接导入库；DLL 与 exe 输出在同一构建目录。测试默认不构建：

```sh
cmake -B build -DPENFRAMEWORK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows / MSVC（本机已验证）

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1            # 编译 + 跑 6 个测试
powershell -ExecutionPolicy Bypass -File build.ps1 -Debug     # 带断言（/Od /MDd）再跑一遍
powershell -ExecutionPolicy Bypass -File build.ps1 -Bench     # 附带跑性能基准
```

脚本通过 `vswhere` 定位 MSVC，用 `/std:c++latest /W4 /WX` 编译，测试二进制在 `build/`。
`build.ps1` 是独立编译路径：它直接编译 6 个实现文件 + 测试源码，不经过 CMake，也不生成 DLL。

### POSIX

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPENFRAMEWORK_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
# 或者手工：
g++ -std=c++23 -O2 -pthread -I. Interface.cpp ThreadCache.cpp CentralCache.cpp \
    PageCache.cpp Radix.cpp OSMemory.cpp Test/UnitTest/test_interface.cpp -o test_interface
```

## 4. 三层各自的职责

| 层 | 关键数据结构 | 关键行为 |
| --- | --- | --- |
| `ThreadCache` | `FreeList m_lists[49]`（侵入式单链表 + `m_lowWater/m_maxLength/m_lengthOverages`） | 命中即返回；`MaxLength` 慢启动收敛到 `NumObjectsToMove`；`Scavenge()` 按 low-water mark 归还一半；全局预算 `m_overallThreadCacheSize` 在 `m_threadCacheLock` 下按 `StealAmount` 分配/偷取 |
| `CentralCache` | 每个 size class：`SpinLock + m_nonEmpty` span 链表 + 对象计数 | `RemoveRange` 从 span 取对象，取空则 `Populate()`（**先解锁**再找页堆）；`InsertRange` 先在锁外用 `PageMap` 把对象归属到 span，再逐个回填；span 全空即还回页堆；`ObjectsPerSpan()==1` 的类直通页堆 |
| `PageCache` | `m_freeLists[128]`（精确长度）+ `m_largeList` + `m_releasedList` + `Span` 对象池 | `New(n)` best-fit → `Carve`；`NewAligned` 支持"包含对齐窗口"的空闲 span 并切掉不对齐的头部；`Delete` 用 `PageMap` 找前后邻居做合并；空闲超过 8 MiB 自动回收一半（`Decommit`），再次使用时 `Commit` |

页堆把每个 span 的**每一页**都登记进 `PageMap`（tcmalloc 只登记首页），因此野指针 / interior free
会被识别而不是静默破坏堆；代价是 O(pages) 次写。

## 5. 测试

`build.ps1` 会依次运行：

| 测试 | 覆盖内容 | 检查点数（release） |
| --- | --- | --- |
| `test_sizemap` | 尺寸类表自洽、size→class 单调且最小、对齐查表、页算术 | 796 011 |
| `test_pagecache` | 切分/合并、对齐 span、跨 chunk 大分配、Release→Commit、span 对象空闲链表、`PageMap` 覆盖整叶 | 640 |
| `test_centralcache` | 批量取还、span 生命周期、单对象 span、每个 size class 全量填/放空 | 419 |
| `test_threadcache` | 线程缓存创建/回收、预算、Scavenge、多线程各自持有缓存、`BecomeIdle` | 2 097 |
| `test_interface` | 全部 size class 往返、1..4 KiB 全尺寸、随机尺寸不重叠、0 字节/nullptr、大对象、对齐分配 8 B..1 MiB、跨线程释放、4 线程随机压力（8 万步） | 27 531 |
| `test_override` | 用 `Code/Native/Engine/Memory/MemoryOperator.cpp` 的生产版本把 `operator new/delete`（含 sized / aligned / nothrow）接到 PenMemory，跑 STL 容器、`make_shared`、over-aligned 类型、多线程容器 | 7 015 |

每个用例结束后都调用 `VerifyHeap()`，它会在持锁状态下校验：空闲链表分桶正确、
span 每一页都映射到自己、相邻空闲 span 已合并、页数/对象数记账一致、对象空闲链表无环且计数正确。

CMake 侧打开 `PENFRAMEWORK_BUILD_TESTS` 后，ctest 运行同一批用例；其中 `test_override` 链接
`PenMemory.dll`，只使用 C ABI 与 `Common.hpp` 里的 header-only 常量/内联函数，其余用例直接编译源码。

## 6. 性能（本机 MSVC /O2，4M 次 alloc+free 循环，64 个对象工作集）

```
  PenMemory      32 B : 145.20 Mops/s      platform malloc : 36.97 Mops/s   (3.93x)
  PenMemory     128 B : 154.39 Mops/s      platform malloc : 37.28 Mops/s   (4.14x)
  PenMemory    8192 B : 128.45 Mops/s      platform malloc : 41.37 Mops/s   (3.10x)
  PenMemory  131072 B : 128.81 Mops/s      platform malloc : 13.60 Mops/s   (9.47x)
  4 线程 32 B: PenMemory 473 Mops/s vs platform 101 Mops/s
```

`pwsh build.ps1 -Bench` 可复现。基准只用于确认快路径足够短，不是严谨的对标。

## 7. 与 tcmalloc 的差异

| 项 | tcmalloc | 本实现 |
| --- | --- | --- |
| 层数 | 4（CpuCache/ThreadCache/TransferCache/CentralFreeList） | 3（TransferCache 合并进 CentralCache） |
| span 内空闲对象 | 2 字节 index + 内嵌 cache 数组 | 侵入式单链表（对象首字存 next） |
| size class | 冗余存进 PageMap | 存在 `Span::m_sizeClass` |
| nonempty 分级 | 按 `bit_width(allocated)` 分 NumLists 桶 | 单条 `m_nonEmpty` 链表 |
| 页堆 | HPAA + hugepage filler + subrelease | 经典 PageHeap（按长度分桶 + 邻居合并） |
| 归还 OS | 后台线程按 release rate 回收 | 空闲超过阈值自动 `Decommit` 一半到 released 链表，按需 `Commit` |
| 平台细节 | 不区分（Linux 为主） | Windows 上游离出"OS reservation（region）"概念：**跨 region 的空闲 span 不合并**，因为 `MEM_DECOMMIT` 不能跨越两次 `VirtualAlloc` 的边界 |

`DESIGN.md` 里有完整的源码理解汇报与设计推导过程。
