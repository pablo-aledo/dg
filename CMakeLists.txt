cmake_minimum_required(VERSION 2.8)
project(dg)

include(CTest)

include_directories( ${CMAKE_SOURCE_DIR}/src )

# define DEBUG by default for now
OPTION(ENABLE_DEBUG "Enable debugging" ON)
OPTION(LLVM_DG "Support for LLVM Dependency graph" ON)
OPTION(ENABLE_CFG "Add support for CFG edges to the graph" ON)

if (LLVM_DG)
	# for llvm dg we need cfg and postdom edges
	if (NOT ENABLE_CFG)
		message(STATUS "Enabling CFG edges due to llvm dg")
	endif()

	set(ENABLE_CFG ON)

	find_package(LLVM REQUIRED CONFIG)

	message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
	message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
	include(LLVMConfig)

	message(STATUS "LLVM include dir: ${LLVM_INCLUDE_DIRS}")
	message(STATUS "LLVM libraries dir: ${LLVM_LIBRARY_DIRS}")
	message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

	if (LLVM_SRC_PATH)
	  include_directories(${LLVM_SRC_PATH}/include)
	  message(STATUS "Looking for headers in given: ${LLVM_SRC_PATH}/include")
	else()
	  include_directories(${LLVM_INCLUDE_DIRS})
	  message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")
	endif()

	if (LLVM_BUILD_PATH)
	  link_directories(${LLVM_BUILD_PATH}/lib)

	  # llvm-config.h
	  include_directories("${LLVM_BUILD_PATH}/include")
	  message(STATUS "Looking for libraries in given: ${LLVM_BUILD_PATH}/lib")
	else()
	  link_directories(${LLVM_LIBRARY_DIRS})
	  message(STATUS "Looking for libraries in: ${LLVM_LIBRARY_DIRS}")
	endif(LLVM_BUILD_PATH)

	add_definitions(${LLVM_DEFINITIONS})
	llvm_map_components_to_libraries(llvm_libs support core irreader bitwriter analysis)

	add_definitions(-DHAVE_LLVM)
endif(LLVM_DG)

if (ENABLE_CFG)
	add_definitions(-DENABLE_CFG)
endif()

if (ENABLE_DEBUG)
	add_definitions(-DDEBUG_ENABLED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fno-rtti -std=c++11 -g -O0")
else ()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fno-rtti -std=c++11 -O2")
endif ()

# generate a git-version.h with a HEAD commit hash tag
add_custom_target(
	gitversion ALL sh git-version.sh > git-version.h
)

add_subdirectory(src)
add_subdirectory(tests EXCLUDE_FROM_ALL)
add_subdirectory(tools)
