# File CMakeLists.txt
#
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

cmake_minimum_required(VERSION 4.0)

project("PenFramework")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

message(STATUS "CMake版本：${CMAKE_VERSION}")
message(STATUS "CMake编译类型：${CMAKE_BUILD_TYPE}")
message(STATUS "编译器版本：${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "开始设置编译器特殊选项")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") 
    if(PENFRAMEWORK_CMAKE_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug") 
        message(STATUS "启用ASAN")
        add_compile_options(/fsanitize=address)
        add_compile_definitions(PENFRAMEWORK_ENABLE_ASAN)
    else()
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
    endif()

    add_compile_options(/utf-8) 
    add_compile_options(/Zc:preprocessor)
    add_compile_options(/diagnostics:caret)
    add_compile_options(/FC)
else()
    message(STATUS "该编译器不需要特殊选项")
endif()

message(STATUS "编译器特殊选项设置完毕")

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)

    message(STATUS "工作平台：Windows")
    message(STATUS "Windows子系统类型：${PENFRAMEWORK_CMAKE_WIN_SUBSYSTEM}")

    string(TOUPPER "${PENFRAMEWORK_CMAKE_WIN_SUBSYSTEM}" PENFRAMEWORK_CMAKE_WIN_SUBSYSTEM_TYPE)

    add_compile_definitions(PENFRAMEWORK_WIN_SUBSYSTEM_${PENFRAMEWORK_CMAKE_WIN_SUBSYSTEM_TYPE})
endif()

# 包管理
# ---------------------------------------------
# String Character Convert | boost::locale 支持
find_package(Boost CONFIG REQUIRED COMPONENTS
	locale
)

file(GLOB_RECURSE PEN_SOURCES Code/Native/*)

add_executable(${PROJECT_NAME} ${PEN_SOURCES})

target_link_libraries(${PROJECT_NAME}  PUBLIC 
    Boost::locale 
)

# 编译对象
message(STATUS "编译对象：${PENFRAMEWORK_CMAKE_TARGET}")

string(TOUPPER "${PENFRAMEWORK_CMAKE_TARGET}" PENFRAMEWORK_CMAKE_TARGET_TYPE)
add_compile_definitions(PENFRAMEWORK_TARGET_${PENFRAMEWORK_CMAKE_TARGET_TYPE})