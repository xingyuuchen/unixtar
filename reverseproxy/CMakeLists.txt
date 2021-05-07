cmake_minimum_required(VERSION 3.4)
project(reverse-proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} *.cc)
list(APPEND REVERSE_PROXY_SRC ${SELF_TEMP_SRC_FILES})

if (${DAEMON})
    add_definitions("-DDAEMON")
endif()


add_executable(${PROJECT_NAME} ${REVERSE_PROXY_SRC})

add_dependencies(${PROJECT_NAME} framework)

target_link_libraries(${PROJECT_NAME} framework)
