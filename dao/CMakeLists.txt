cmake_minimum_required(VERSION 3.4.1)
project(oi-svr-dao)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(.)
include_directories(/usr/include/mysql)
include_directories(/usr/local/include/mysql++)

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ../.. *.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

#link_directories(/usr/local/lib)
#
#add_library(${PROJECT_NAME} SHARED ${SELF_SRC_FILES})
#
#target_link_libraries(${PROJECT_NAME} mysqlpp)
