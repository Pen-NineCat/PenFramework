# PenMemory 设计文档（源码理解汇报 + 实现方案）

本文档分两部分：

1. **上半部分**：阅读 `tcmalloc/`（google/tcmalloc 主干）源码后的理解汇报；
2. **下半部分**：本次要实现的三层简化版 tcmalloc 的设计与取舍。

> 模块位置：本文档描述的分配器现在是 `PenFramework/Code/Native/Memory`，由根 `CMakeLists.txt`
> 通过 `add_subdirectory` 构建成 `PenMemory.dll`，主程序通过导入库链接，DLL 与 exe 输出在同一
> 构建目录。对外是纯 C 的 `extern "C"` ABI（`Interface.h`，实现是 `Interface.cpp`），C++ 侧封装在
> `Code/Native/Engine/Memory/Memory.hpp`，全局 `operator new/delete` 接管在
> `Code/Native/Engine/Memory/MemoryOperator.cpp`（只替换主程序自己的 new/delete）。
> 测试由根 `CMakeLists.txt` 的统一选项 `PENFRAMEWORK_BUILD_TESTS`（默认 OFF，约定见
> `Docs/Testing.md`）打开后用 ctest 运行，`build.ps1` 也仍可
> 独立编译并运行测试。文档里出现的旧接口名已按新命名更新，例如
> `CentralFreeList::length()` → `CachedObjectCount()`、`os::PageSize()` → `os::GetSystemPageSize()`。

---

## 一、对 tcmalloc 源码的理解

### 1.1 整体分层

主干 tcmalloc 的分配路径是一个四级缓存结构，每一级都在"更慢但更省内存"的方向上退让：

```
malloc/free (tcmalloc.cc)
   │
   ├─ 小对象 (size <= MaxSize)
   │     CpuCache (per-CPU, 无锁)        cpu_cache.h
   │        └─ ThreadCache (per-thread)  thread_cache.h
   │              └─ TransferCache       transfer_cache.h  ← 批量中转站（按 L3 分片）
   │                    └─ CentralFreeList  central_freelist.h ← 按 size class 的中心空闲链表
   │                          └─ PageAllocator(HPAA)/PageHeap  ← 按 page 管理 span
   │
   └─ 大对象 (size > MaxSize)
         直接走 PageAllocator（以 page 为单位）
```

- `tc_globals`（`static_vars.h`）持有全部全局单例：`sizemap()`、`cpu_cache()`、`transfer_cache()`、
  `page_allocator()`、`pagemap()`、`arena()`。
- `PageMap`（`pagemap.h`）是三级基数树 `PageMap3<AddressBits-PageShift>`，把 **page 号 → Span\*** 做 O(1)
  映射；同时**冗余存了一份 size class**，使 `free(ptr)` 不需要 size 参数、也不需要 load Span 就能拿到 size class。
- `Span`（`span.h`）是一段连续的 page，是三层之间的"货币"。

### 1.2 关键算法（本次要复刻的部分）

| 机制 | 源码位置 | 要点 |
| --- | --- | --- |
| 尺寸类表 | `size_classes.cc` | `PageShift==13`（8 KiB page）时 49 个类，`{size, span_bytes, batch}`；≤1024 按 8 对齐、>1024 按 128 对齐 |
| size→class 映射 | `sizemap.cc::Init` | 一张 `m_classArray` 平铺数组，两段不同对齐用算术压平成一个数组，O(1) |
| 对齐分配 | `sizemap.h::GetSizeClass` | 先按 size 找到类，再 `while (!IsAlignedTo(ClassToSize(c), align)) ++c;`，保证 `class_size % align == 0` |
| ThreadCache | `thread_cache.h/.cc` | `FreeList m_lists[NumClasses]`，含 `m_lowWater/m_maxLength/m_lengthOverages`；`TryPop` 命中即返回（**分配路径无锁、无系统调用**）；`MaxLength` 慢启动（每次 +1，到 batch_size 后按 batch 增长）；`Scavenge` 按 low-water mark 回收一半 |
| 批量搬运 | `thread_cache.cc` | 取用 `NumObjectsToMove(class)` 个对象，减少 central cache 加锁次数 |
| 全局 thread cache 预算 | `thread_cache.cc` | `m_overallThreadCacheSize` 在所有线程间瓜分，`StealAmount` 步进"偷"额度，避免线程缓存无上限增长 |
| CentralFreeList | `central_freelist.h` | 每个 size class 一把锁 + 若干 span；`RemoveRange` 从 nonempty 列表取对象，不够就 `Populate`（**先解锁再加锁**去 PageHeap 拿新 span）；`InsertRange` 先把对象映射到 span（锁外做），再逐个 `ReleaseToSpans`；span 全空则还给 PageHeap |
| span 内空闲对象组织 | `span.cc` | 用 2 字节 index 代替 8 字节指针 + Span 内嵌 4~12 个元素的 cache 数组；对象本身当数组宿主，减少 pointer chasing |
| nonempty 分级 | `central_freelist.h::IndexFor` | 按 `bit_width(allocated)` 把 span 分到 `NumLists` 个链表，优先使用"快空"的 span，提高 span 回收率 |
| PageHeap / HPAA | `page_allocator.h`、`huge_page_aware_allocator.h` | 按长度分桶的空闲 span 链表（<`MaxPages` 精确长度、否则 large list）；`Delete` 时通过 `PageMap` 找前后邻居做**合并**；`GrowHeap` 以 `kMinMmapAlloc`(1 GiB) 为单位向 OS 要内存 |
| 归还 OS | `page_allocator` + `ReleaseAtLeastNPages` | 只把物理页归还（`madvise(MADV_DONTNEED)`），保留地址空间；由后台线程按 release rate 慢速回收 |
| 元数据分配 | `arena.h` / `metadata_allocator.h` | Span、`PageMap` 节点等元数据从独立 arena 分配，**绝不递归调用 malloc** |

### 1.3 `allocate / deallocate` 快慢路径

- `Allocate`：`fast_alloc` → 尺寸类查表 → CpuCache/ThreadCache 弹出；失败再走 `slow_alloc_small`；`size > MaxSize` 走 `slow_alloc_large` → `do_malloc_pages`（按 page 取 span，`ptr = span->StartAddress()`）。
- `Deallocate`：**不需要 size 参数**——`PageIdContaining(ptr)` → `PageMap` 里查 size class：
  - `sizeClass != 0` → 小对象 → 交给 CpuCache/ThreadCache 的对应 FreeList；
  - `sizeClass == 0` → 大对象 → `InvokeHooksAndFreePages`（校验 `ptr == span->StartAddress()`）→ `PageAllocator::Delete`。

---

## 二、PenMemory 设计（本次实现）

### 2.1 目标与范围

- **只做三层**：`ThreadCache` → `CentralCache` → `PageCache`，**无** per-CPU cache、**无** 采样/profiling、**无** NUMA/security partition、**无** hugepage(HPAA)。
- **per-thread 分配**：每个线程一份 `ThreadCache`，用 `thread_local` 指针做快路径，用 OS 级 TLS 析构回调做线程退出清理（与 tcmalloc 的 `__thread` + `pthread_setspecific` 组合同构）。
- 对外只暴露 4 个函数：`Allocate / Deallocate / AlignedAllocate / AlignedDeallocate`（纯 C 的 `extern "C"` ABI）。
- C++23，MSVC(`/std:c++latest`) / GCC / Clang 均可编译，Windows 用 `VirtualAlloc*`，Linux 用 `mmap/munmap/madvise`。

### 2.2 文件结构

```
Code/Native/Memory/
  Common.hpp        配置常量、Length/PageId/Range、尺寸类表、SizeMap、SpinLock、断言
  OSMemory.h/.cpp   OS 内存原语 + 元数据 arena（对应 tcmalloc 的 arena/metadata_allocator）
  Radix.h/.cpp      三级基数树 RadixTree + PageMap（对应 pagemap.h + PageMap3）
  PageCache.h/.cpp  Span 定义 + 页堆（按长度分桶、邻居合并、归还 OS）
  CentralCache.h/.cpp  每个 size class 的中心空闲链表（CentralFreeList + 批量中转）
  ThreadCache.h/.cpp   线程本地缓存（FreeList 数组 + 慢启动 + Scavenge + 全局预算）
  Interface.h/.cpp     对外 C ABI（`extern "C"`）的实现：4 个入口 + 统计
  Globals.hpp       全局单例 Globals（等价 tc_globals，内部 C++ 状态，不进 C ABI）
  Test/UnitTest/     6 个测试程序
  build.ps1 / CMakeLists.txt / README.md
```

> 说明：相对推荐结构额外增加了 `Common.hpp`（公共类型与尺寸类，被所有层包含）。`Span` 放在
> `PageCache.h` 中，因为 span 正是页堆的分配单位，CentralCache 通过 `PageCache.h` 自然获得它；
> `Globals`（等价于 `tc_globals`）放在 `Globals.hpp`，因为它是最上层对全局单例的组合，
> 同时它属于内部 C++ 状态，不进导出的 C ABI。

### 2.3 与 tcmalloc 的差异（有意简化）

| 项 | tcmalloc | PenMemory | 原因 |
| --- | --- | --- | --- |
| 缓存层数 | 4 层（CPU/Thread/Transfer/Central） | 3 层，TransferCache 合并进 CentralCache | 需求只要三层 |
| span 内空闲对象 | 2 字节 index + 内嵌 cache 数组 | 经典侵入式单链表（对象首 8 字节存 next） | 实现简单、易验证 |
| size class 存放 | `PageMap` 里冗余一份（省一次 load） | 存在 `Span::m_sizeClass` | 简化 Radix 的值类型 |
| `PageMap` 覆盖范围 | 大对象 span 只登记首页 | **登记 span 的每一页** | 便于识别野指针/interior free，代价 O(pages) 次写 |
| nonempty 分级 | `NumLists` 个链表按 bit_width 分桶 | 单条 `m_nonEmpty` 链表 | 简化 |
| PageHeap | HPAA + hugepage filler + subrelease | 经典 PageHeap：按长度分桶 + 邻居合并 | 简化 |
| NewAligned | 只接受"首页已对齐"的空闲 span，靠重试/扩容 | 额外接受**包含对齐窗口**的空闲 span，并把不对齐的头部切回空闲链表 | 大对齐（如 512 页）无需反复扩容即可满足 |
| 归还 OS | madvise + 后台线程按速率回收 | `ReleaseAtLeastNPages`：把空闲 span `Decommit` 进 released 链表，按需再 `Commit`；空闲页超过阈值（8 MiB）时自动回收一半 | 保留"归还 OS"能力，去掉后台线程 |
| 大对象 | 直接向 PageHeap 要 span | 相同（`size > MaxSize` 走 PageCache） | 一致 |

### 2.6 实现中发现的平台约束（已体现在代码里）

1. **Windows 的 `MEM_DECOMMIT` 不能跨越两次 `VirtualAlloc` 的边界**。
   页堆按地址相邻来合并空闲 span，而 OS 经常把新的 chunk 分配在旧 chunk 紧邻的位置，
   合并后的 span 会横跨两个 reservation，`VirtualFree(MEM_DECOMMIT)` 于是失败。
   解决：`Span` 记录它所属的 **region id**（每次 `os::Allocate` 登记一个 region），
   合并只在同一个 region 内发生；同时 `os::Decommit` 返回 bool，失败时 span 留在空闲链表里。
2. **`VirtualAlloc` 只保证 64 KiB（分配粒度）对齐**，而本分配器的页是 8 KiB，
   因此 OS 返回的地址天然满足页对齐要求；POSIX 上 `mmap` 只保证 4 KiB 对齐，
   `os::Allocate` 通过"多映射一点再 `munmap` 掐头去尾"来保证 8 KiB 对齐。
3. **`FlsAlloc` 的回调签名是 `void(PVOID)`**（不是 `DllMain` 的三参数形式），
   线程退出清理用 FLS 槽（Windows）/ `pthread_key_create`（POSIX），
   与 tcmalloc 的 `pthread_setspecific` 方案同构。

### 2.7 关键数据结构

- **SizeMap**：`m_classArray[2169]` 做 size→class O(1) 映射；`m_classToSize/m_classToPages/m_numObjectsToMove`。
  采用 `__STDCPP_DEFAULT_NEW_ALIGNMENT__ > 8` 那一版尺寸类表（16 字节对齐友好的
  `8,16,32,48,64,80,96,112,128,160,176,208,256,304,...`），共 49 个类，最大 256 KiB。
- **Span**：`m_firstPage, m_numPages, m_sizeClass, m_state(Free/Allocated), m_freeList, m_numAllocated,
  m_next/m_prev(空闲链表), m_released, m_regionId(所属 OS reservation)`。由 PageCache 用元数据 arena
  分配并回收复用（`Span` 对象只由页堆创建，所以回收池也放在页堆里）。
- **RadixTree<T,BITS>**：三级，`leaf=12bit, mid=12bit, root=11bit`（BITS=35，8 KiB page、48 位地址空间），
  root 数组 2048 项(16 KiB)，叶子覆盖 4096 page = 32 MiB；节点从元数据 arena 分配。
- **PageCache**：`m_freeLists[MaxPages=128]`（精确长度）+ `m_largeList` + `m_releasedList`；
  `New(n)` 走 best-fit→Carve；`Delete(span)` 用 `PageMap` 找前后邻居做合并。
- **CentralFreeList**：`SpinLock + m_nonEmpty` 链表 + `m_pagesPerSpan/m_objectsPerSpan`；
  `RemoveRange` / `InsertRange` 与 tcmalloc 同构（含 `ObjectsPerSpan()==1` 时直通页堆的特例）。
- **ThreadCache**：`FreeList m_lists[49]` + `m_size/m_maxSize` + 全局预算 `m_overallThreadCacheSize`。

### 2.8 并发与锁

- `ThreadCache` 快路径**完全无锁**（只有本线程访问自己的 `FreeList`）。
- `m_threadCacheLock`：保护线程缓存链表、全局预算、ThreadCache 对象池（只在创建/销毁/Scavenge 时使用）。
- 每个 `CentralFreeList` 一把自旋锁；`Populate` 时**先解锁**再访问 PageCache（与 tcmalloc 相同，避免锁嵌套）。
- `PageCache` 一把自旋锁，内部自洽，不与上层锁嵌套 → **无锁序问题**。
- 元数据 arena 一把自旋锁（只在创建新 chunk/节点时使用）；唯一的嵌套是
  `PageCache 锁 → arena 锁`，方向固定。

### 2.9 正确性要点（测试会覆盖）

1. `AlignOf(ptr) >= align`（`align ≤ PageSize` 用对齐尺寸类；`align > PageSize` 用页级对齐 span）。
2. 分配对象**不重叠**：小对象来自 span 内固定步长切分；页级来自不重叠的 span。
3. `Deallocate(nullptr)` 安全；重复释放/野指针会被检测并报错。
4. 跨线程释放（A 线程分配、B 线程释放）正确：走 `span->SizeClass()` 定位，再进入**释放线程**的 ThreadCache。
5. span 全空后归还 PageCache，邻居合并，空闲页可 `Decommit` 并在再次分配时 `Commit` 复用。
6. 大对象（> 256 KiB）按 page 取整，指针即 span 首地址，释放后合并回页堆。

---

## 三、实现结果

- 6 个测试程序全部通过（release `/O2` 与 debug `/Od`＋全断言两种配置各跑一遍，
  另外连续重复运行 5 轮无失败）；每个用例结束都做一次全堆不变量校验。
  检查点数量：size map 796 011、page cache 640、central cache 419、thread cache 2 097、
  interface 27 531、global new/delete 7 015。
- 基准（MSVC `/O2`，4M 次 alloc+free，64 对象工作集）：
  32 B / 128 B / 8 KiB 分别比平台 malloc 快 **3.9x / 4.1x / 3.1x**，
  131072 B（单对象 span，直通页堆）快 **9.5x**；4 线程 32 B 达到 473 Mops/s。
- 测试过程中抓到的两个真实缺陷（都已修）：
  1. `SearchFreeLists` 在 large list 上只按对齐条件返回候选，**漏了 `NumPages() >= n` 判断**，
     导致大对象 span 被"撑大"到超出真实范围（`New(4096)` 返回了 515 页的 span）；
  2. 跨 `VirtualAlloc` reservation 合并空闲 span，使 `MEM_DECOMMIT` 失败（见 2.6 第 1 条）。
