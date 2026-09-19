# PenMemory：PenFramework 的内存分配器（由 PersonalWorkaroud/mini-tcmalloc 移植而来）
# ----------------------------------------------------------------------------
# 由根 CMakeLists.txt 通过 add_subdirectory(Code/Native/Memory) 引入，
# 构建成 PenMemory.dll，主程序通过导入库 PenMemory.lib 调用它的 C ABI
# （Interface.h 里 extern "C" 的那组函数）。
#
# 子目录不作为独立工程使用：cmake_minimum_required / project / 编译选项
# 全部沿用根 CMakeLists.txt 的设置（C++23、MSVC /utf-8 /Zc:preprocessor 等）。

set(PEN_MEMORY_SOURCES
	Interface.cpp
	ThreadCache.cpp
	CentralCache.cpp
	PageCache.cpp
	Radix.cpp
	OSMemory.cpp
)

add_library(PenMemory SHARED ${PEN_MEMORY_SOURCES})

# 构建 DLL 时导出 C ABI；链接这个目标的一方自动拿到 dllimport
target_compile_definitions(PenMemory PRIVATE PEN_MEMORY_BUILD)
target_compile_definitions(PenMemory INTERFACE PEN_MEMORY_IMPORT)

# 测试直接包含模块内的内部头（Globals.hpp / PageCache.h ...）
target_include_directories(PenMemory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# DLL 必须与 PenFramework.exe 在同一目录才能被加载
set_target_properties(PenMemory PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(NOT WIN32)
	find_package(Threads REQUIRED)
	target_link_libraries(PenMemory PRIVATE Threads::Threads)
endif()

# ----------------------------------------------------------------------------
# 测试与基准（默认关闭；打开后可用 ctest 运行）
# ----------------------------------------------------------------------------
option(PEN_MEMORY_BUILD_TESTS "构建 PenMemory 的测试与基准程序" OFF)

if(PEN_MEMORY_BUILD_TESTS)
	# 内部测试：直接编译分配器源码，可以访问 SizeMap/PageCache 等内部类型
	function(pen_memory_internal_test name)
		add_executable(${name} ${ARGN} ${PEN_MEMORY_SOURCES})
		target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
		set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
		add_test(NAME ${name} COMMAND ${name})
	endfunction()

	# 边界测试：只使用导出的 C ABI，验证 DLL 的导入/导出
	function(pen_memory_dll_test name)
		add_executable(${name} ${ARGN})
		target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
		target_link_libraries(${name} PRIVATE PenMemory)
		set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
		add_test(NAME ${name} COMMAND ${name})
	endfunction()

	pen_memory_internal_test(test_sizemap       tests/test_sizemap.cpp)
	pen_memory_internal_test(test_pagecache     tests/test_pagecache.cpp)
	pen_memory_internal_test(test_centralcache  tests/test_centralcache.cpp)
	pen_memory_internal_test(test_threadcache   tests/test_threadcache.cpp)
	pen_memory_internal_test(test_interface     tests/test_interface.cpp)

	# test_override 链接 DLL，并使用主程序那套全局 operator new/delete
	pen_memory_dll_test(test_override
		tests/test_override.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Memory/MemoryOperator.cpp
	)
	# C ABI 本身（不接管 operator new/delete）
	pen_memory_dll_test(test_dll tests/test_dll.cpp)

	# 基准（只构建，不由 ctest 运行）
	add_executable(benchmark tests/benchmark.cpp ${PEN_MEMORY_SOURCES})
	target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	set_target_properties(benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
