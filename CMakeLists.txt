cmake_minimum_required(VERSION 4.0)

project("PenFramework")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

message(STATUS "Cmake version: ${CMAKE_VERSION}.")
message(STATUS "Cmake build type: ${CMAKE_BUILD_TYPE}.")

message(STATUS "Set compiler: ${CMAKE_CXX_COMPILER_ID} sepcific options.")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") 
    if (POLICY CMP0141)
      cmake_policy(SET CMP0141 NEW)
      set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
    endif()

    add_compile_options(/utf-8) 
    add_compile_options(/Zc:preprocessor)
    add_compile_options(/diagnostics:caret)
    add_compile_options(/FC)
else()
    message(STATUS "Compiler dont need sepcific options.")
endif()

message(STATUS "Compiler sepcific options ready.")

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 包管理 
# ---------------------------------------------
# Char convert 支持
find_package(Boost CONFIG REQUIRED COMPONENTS
	locale
)
# boost::unordered_flat_map 支持
find_package(Boost CONFIG REQUIRED COMPONENTS
    unordered
)
# Coroutine System 支持
# 如果std::hive可用，可以使用std::hive替换
find_path(PLF_HIVE_INCLUDE_DIRS "plf_hive.h")
# ---------------------------------------------

# PenMemory 分配器：Code/Native/Memory → PenMemory.dll
# ---------------------------------------------
# 该目录是独立模块，由它自己的子 CMakeLists.txt 构建；主程序只链接它的导入库
option(PEN_MEMORY_BUILD_TESTS "构建 PenMemory 的测试与基准程序" OFF)
if(PEN_MEMORY_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(Code/Native/Memory)
# ---------------------------------------------

# 主程序源码：Main.cpp 与 Engine/**；Memory 目录不参与主程序编译，避免同一份分配器被编两遍
file(GLOB_RECURSE SRC ${CMAKE_SOURCE_DIR}/Code/Native/Engine/*)
list(APPEND SRC ${CMAKE_SOURCE_DIR}/Code/Native/Main.cpp)

add_executable(${PROJECT_NAME} ${SRC})

# 包管理 
# ---------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Boost::locale 
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Boost::unordered
)
target_include_directories(${PROJECT_NAME} PRIVATE ${PLF_HIVE_INCLUDE_DIRS})
# ---------------------------------------------

# PenMemory 分配器（PenMemory.dll + 导入库）
# ---------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE PenMemory)
# ---------------------------------------------

# Visual Studio Nativs 支持
# ---------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Nativs Visualizer ready.")
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/Visualizers/PenFramework.natvis")
endif()
# ---------------------------------------------