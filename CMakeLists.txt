cmake_minimum_required(VERSION 3.4)
project(framework)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g2 -ggdb -save-temps=obj")
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

include_directories(.)
include_directories(autogen)
include_directories(dao)
include_directories(http)
include_directories(longlink)
include_directories(netscene)
include_directories(networkmodel)
include_directories(socket)
include_directories(utils)

include_directories(/usr/local/include)

add_subdirectory(utils)

# Before compiling this subproject, make sure mysql++ installed.
add_subdirectory(dao)

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} *.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} autogen/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} http/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} longlink/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} netscene/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} networkmodel/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} reverseproxy/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

file(GLOB SELF_TEMP_SRC_FILES RELATIVE ${PROJECT_SOURCE_DIR} socket/*.cc)
list(APPEND SELF_SRC_FILES ${SELF_TEMP_SRC_FILES})

if (${DAEMON})
    add_definitions("-DDAEMON")
endif()

link_directories(/usr/local/lib)

add_library(${PROJECT_NAME} SHARED ${SELF_SRC_FILES})

add_dependencies(${PROJECT_NAME} utils dao)

target_link_libraries(${PROJECT_NAME} protobuf utils dao)

add_subdirectory(reverseproxy)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND test -d /etc/unixtar || mkdir -p /etc/unixtar
            COMMAND test -e ${PROJECT_SOURCE_DIR}/dao/dbconfig.yml || echo "Please create dbconfig.yml in framework/dao/"
            COMMAND cp ${PROJECT_SOURCE_DIR}/webserverconf.yml /etc/unixtar/webserverconf.yml
            COMMAND cp ${PROJECT_SOURCE_DIR}/reverseproxy/proxyserverconf.yml /etc/unixtar/proxyserverconf.yml
            COMMAND cp ${PROJECT_SOURCE_DIR}/dao/dbconfig.yml /etc/unixtar/dbconfig.yml
            COMMENT "Copying unixtar config files...")
endif()
