cmake_minimum_required(VERSION 2.6)

project(TESLA)

set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

#
# CMake helpers.
#
include(cmake/Concatenate.cmake)
include(cmake/CopyFiles.cmake)

# Include LLVM configuration:
find_package(LLVM REQUIRED)
include(AddLLVM)

if(NOT CMAKE_LLVM_CONFIG)
	set(CMAKE_LLVM_CONFIG llvm-config)
endif()

if(NOT CMAKE_LLVM_LIT)
	set(CMAKE_LLVM_LIT llvm-lit)
endif()

# Make sure we can find llvm-config!
exec_program(${CMAKE_LLVM_CONFIG} ARGS --version  OUTPUT_VARIABLE LLVM_VERSION)
if("${LLVM_VERSION}" MATCHES "not found")
  message(FATAL_ERROR "llvm-config not found; may need to set PATH?")
endif()

exec_program(${CMAKE_LLVM_CONFIG} ARGS --includedir OUTPUT_VARIABLE LLVM_INCDIR)
exec_program(${CMAKE_LLVM_CONFIG} ARGS --libdir   OUTPUT_VARIABLE LLVM_LIBDIR)
exec_program(${CMAKE_LLVM_CONFIG} ARGS --src-root OUTPUT_VARIABLE LLVM_SRC)
exec_program(${CMAKE_LLVM_CONFIG} ARGS --obj-root OUTPUT_VARIABLE LLVM_OBJ)

# Find the Clang "resource dir" (contains include/stdarg.h, etc.).
if(NOT CLANG_RESOURCE_DIR)
	exec_program(${CMAKE_LLVM_CONFIG} ARGS --bindir OUTPUT_VARIABLE LLVM_BIN)
	exec_program(${LLVM_BIN}/clang ARGS --version OUTPUT_VARIABLE VERSION)

	string(REPLACE "clang version " "" VERSION ${VERSION})
	string(REGEX REPLACE "\n.*" "" VERSION ${VERSION})
	string(REGEX REPLACE " .*" "" VERSION ${VERSION})

	set(CLANG_RESOURCE_DIR ${LLVM_OBJ}/lib/clang/${VERSION}
		CACHE PATH "Clang resource dir (stdarg.h, etc.)")
	message(STATUS "Found Clang resource dir: ${CLANG_RESOURCE_DIR}")
endif()

# Explicitly put LLVM's library directory before others, e.g. /usr/local/lib.
link_directories(${LLVM_LIBDIR})

# Always build with all warnings.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wstrict-prototypes")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Disable RTTI usage in protocol buffers.
#
# This seems to "just work" on FreeBSD and Mac OS X, but not always on Linux...
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D GOOGLE_PROTOBUF_NO_RTTI")

#
# TESLA uses C++11.
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#
# Global include directories for all of TESLA:
#
include_directories(${LLVM_INCDIR})
# Add includes from src and obj only if we're building from a
# non-installed copy.
if (${LLVM_SRC}/include STREQUAL ${LLVM_INCDIR})
	include_directories(
		${LLVM_OBJ}/include
		${LLVM_SRC}/tools/clang/include
		${LLVM_OBJ}/tools/clang/include
	)
endif()
include_directories(
	include                          # [lib]tesla.h
	${CMAKE_BINARY_DIR}/include      # config.h
)


# Default to libc++ if installed.
find_package(LibCXX)

set(USE_LIBCXX ${LIBCXX_FOUND} CACHE BOOL "Use libc++ when building TESLA")
if (USE_LIBCXX)
  message(STATUS "Using libc++")
else ()
  message(STATUS "Using the default system STL implementation")
endif ()

if (USE_LIBCXX)
  set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -stdlib=libc++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

  if( ${CMAKE_SYSTEM_NAME} MATCHES FreeBSD )
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -l cxxrt")
  endif ()

  include_directories(${LIBCXX_INCLUDE_DIRS})
endif ()


#
# Set up some variables to control the testing framework:
#
set(LIT_EXTRA "--param=extra_cxxflags=${CMAKE_CXX_FLAGS}")

set(SHOW_TEST_STATS false CACHE BOOL "Show statistics after running tests")
if (SHOW_TEST_STATS)
  message(STATUS "Will show statistics after each test run")
  set(LIT_OPTIONS "-sv")
else ()
  message(STATUS "Will run successful tests quietly")
  set(LIT_OPTIONS "-qv")
endif ()

set(EXPORT_XUNIT_XML true CACHE BOOL "Export test results as XUnit XML")
if (EXPORT_XUNIT_XML)
  message(STATUS "Will export test results XUnit XML")
  set(LIT_EXTRA "--xunit-xml-output=tesla.xml;${LIT_EXTRA}")
else ()
  message(STATUS "Will not export test results XUnit XML")
endif ()

set(TEST_UNDER_VALGRIND false CACHE BOOL "Run tests under Valgrind")
if (TEST_UNDER_VALGRIND)
  message(STATUS "Tests will be run under Valgrind")
  set(LIT_OPTIONS "${LIT_OPTIONS}" "--vg-leak")
else ()
  message(STATUS "Tests will not be run under Valgrind")
endif ()


#
# Platform-specific hacks for running tests:
#
if( ${CMAKE_SYSTEM_NAME} MATCHES FreeBSD )
	# We use execinfo for detailed failure information (e.g. backtraces).
	#
	# This is only required on FreeBSD, as both Mac OS X and Linux include
	# backtrace functions in libSystem / libc.
	find_package(ExecInfo REQUIRED)
	include_directories(${EXECINFO_INCLUDE_DIRS})

	set(LIT_EXTRA ${LIT_EXTRA}
		--param=extra_include_dirs=${EXECINFO_INCLUDE_DIRS}
		--param=extra_libs="-lthr:${EXECINFO_LIBRARY}")

else ( ${CMAKE_SYSTEM_NAME} MATCHES Linux )
	# On some Linux installations, we need to explicitly pass the
	# -resource-dir flag to Clang tools. The Clang driver handles this
	# automatically, and we need to figure out why the Clang tooling
	# framework doesn't. In the meantime, however, this workaround works.
	set(LIT_EXTRA ${LIT_EXTRA}
		--param=extra_cflags=-resource-dir:${CLANG_RESOURCE_DIR})
endif()

#
# Check for functions that don't exist on every platform:
#
include(CheckFunctionExists)
check_function_exists(issetugid HAVE_ISSETUGID)
check_function_exists(strnlen HAVE_STRNLEN)


#
# Put discovered configuration information into config.h:
#
configure_file(cmake/config.h.in include/config.h)


#
# Indicate that we have a config.h file
#
ADD_DEFINITIONS(-DHAVE_CONFIG_H)


add_subdirectory(doc)
add_subdirectory(demos)
add_subdirectory(include)
add_subdirectory(libtesla)
add_subdirectory(tesla)
add_subdirectory(scripts)

# A top-level 'check' target to drive all tests of TESLA subdirectories.
add_custom_target(check DEPENDS libtesla-test tesla-test run-demos)
