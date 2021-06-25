cmake_minimum_required(VERSION 3.4)
project(utils)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g2 -ggdb")

enable_language(ASM)

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} *.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} coroutine/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} coroutine/*.S)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} threadpool/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} messagequeue/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

if (${DAEMON})
    add_definitions("-DDAEMON")
endif()

add_library(${PROJECT_NAME} SHARED ${SELF_SRC_FILES})

add_executable(testco coroutine/test_producer_consumer.cc coroutine/coroutine.cc coroutine/coroutine_util.S)

