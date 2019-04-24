if (WIN32)
    cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
else()
    cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release
		CACHE STRING "Set build type to Debug or Release (default Release)" FORCE)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "Limited configurations" FORCE)

project (libcorocxx C CXX)

include (CheckFunctionExists)
include (CheckIncludeFiles)
include (CheckIncludeFile)
include (CheckIncludeFileCXX)
include (CheckSymbolExists)

set(SOURCE_FILES_DIR ${PROJECT_SOURCE_DIR}/src)
set(TEST_FILES_DIR ${PROJECT_SOURCE_DIR}/test)
include_directories(BEFORE ${SOURCE_FILES_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/include)

set(SOURCE_FILES
	${SOURCE_FILES_DIR}/coro.cpp
	${SOURCE_FILES_DIR}/coro_impl.cpp
	${SOURCE_FILES_DIR}/coro_stack.cpp)

set(HEADER_FILES 
	${SOURCE_FILES_DIR}/coro.hpp
	${SOURCE_FILES_DIR}/coro_impl.hpp 
	${SOURCE_FILES_DIR}/coro_stack.hpp 
	${CMAKE_CURRENT_BINARY_DIR}/include/config.h)

if(NOT CORO_UCTX AND NOT CORO_LJMP AND NOT CORO_FIBER)
#检查是否能使用汇编实现
	if(UNIX AND NOT APPLE)
		check_symbol_exists(__i386 "features.h" I386_ARCH)
		check_symbol_exists(__x86_64 "features.h" X86_64_ARCH)
		if(I386_ARCH OR X86_64_ARCH)
			set(CORO_ASM 1)
		endif()
	endif()
endif()

if(NOT CORO_ASM AND NOT CORO_FIBER)
#非汇编环境检查是否能使用ucontext()系列函数或longjmp(),sigaltstack()函数实现
	check_include_files (ucontext.h HAVE_UCONTEXT_H)
	check_include_files (setjmp.h HAVE_SETJMP_H)
	check_symbol_exists (sigaltstack "signal.h" HAVE_SIGALTSTACK)
	check_symbol_exists (sigsetjmp "setjmp.h" HAVE_SIGSETJMP)
endif()

if(WIN32)
#由于c++的封装WINDOWS平台仅支持CORO_FIBER，且不能是Cygwin环境的编程
	set(CORO_FIBER 1)
endif()

if(NOT WIN32 AND CORO_FIBER)
	message(FATAL_ERROR "Unsupported to use CreateFiber/SwitchToFiber functions")
endif()

if(${CMAKE_BUILD_TYPE_LOWER} STREQUAL "debug")
	add_definitions(-DDEBUG)
endif()

configure_file(
	${PROJECT_SOURCE_DIR}/config.h.cmake
	${CMAKE_CURRENT_BINARY_DIR}/include/config.h
		NEWLINE_STYLE UNIX)

#判断最终实现
if(CORO_ASM)
	list(APPEND SOURCE_FILES ${SOURCE_FILES_DIR}/coro_asm.cpp)
	message(STATUS "*** USE Hand coded assembly TO IMPLEMENT COROTINE***")
elseif(CORO_UCTX)
	list(APPEND SOURCE_FILES ${SOURCE_FILES_DIR}/coro_uctx.cpp)
	message(STATUS "*** USE SUSv2's get/set/swap/makecontext functions TO IMPLEMENT COROTINE***")
elseif(CORO_LJMP)
	list(APPEND SOURCE_FILES ${SOURCE_FILES_DIR}/coro_ljmp.cpp)
	message(STATUS "*** USE setjmp/longjmp and sigaltstack functions TO IMPLEMENT COROTINE***")
elseif(CORO_FIBER)
	list(APPEND SOURCE_FILES ${SOURCE_FILES_DIR}/coro_fiber.cpp)
	message(STATUS "*** USE CreateFiber/SwitchToFiber functions TO IMPLEMENT COROTINE***")
elseif(HAVE_UCONTEXT_H)
	list(APPEND SOURCE_FILES ${SOURCE_FILES_DIR}/coro_uctx.cpp)
	message(STATUS "*** USE SUSv2's get/set/swap/makecontext functions TO IMPLEMENT COROTINE***")
elseif(HAVE_SETJMP_H)
	list(APPEND SOURCE_FILES ${SOURCE_FILES_DIR}/coro_ljmp.cpp)
	message(STATUS "*** USE setjmp/longjmp and sigaltstack functions TO IMPLEMENT COROTINE***")
else()
	message(FATAL_ERROR "unknown or unsupported architecture")
endif()

source_group("HEADERS" FILES ${HEADER_FILES})
source_group("SOURCES" FILES ${SOURCE_FILES})

add_library(corocxx STATIC ${HEADER_FILES} ${SOURCE_FILES})

set(PLATFORM_LIB)

if(CORO_MUTEX)
	list(APPEND PLATFORM_LIB pthread)
endif()

add_executable(test-coro ${TEST_FILES_DIR}/test-coro.cpp)
target_link_libraries (test-coro corocxx ${PLATFORM_LIB})
