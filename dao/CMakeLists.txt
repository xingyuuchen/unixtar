cmake_minimum_required(VERSION 3.4.1)
project(dao)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(.)
include_directories(/usr/include/mysql)
include_directories(/usr/local/include/mysql)

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} *.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

link_directories(/usr/local/lib)

if (${DAEMON})
    add_definitions("-DDAEMON")
endif()

add_library(${PROJECT_NAME} SHARED ${SELF_SRC_FILES})

add_dependencies(${PROJECT_NAME} utils)

target_link_libraries(${PROJECT_NAME} mysqlpp utils)
