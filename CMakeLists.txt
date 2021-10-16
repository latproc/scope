cmake_minimum_required (VERSION 2.8.12)
project (Scope)

set (Scope_VERSION_MAJOR 0)
set (Scope_VERSION_MINOR 8)

if (EXISTS "${PROJECT_SOURCE_DIR}/LocalCMakeLists.txt")
	include("${PROJECT_SOURCE_DIR}/LocalCMakeLists.txt")
endif()

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")


set (CMAKE_CXX_FLAGS "-g -Wall --std=c++11")

set (CLOCKWORK_DIR ${PROJECT_SOURCE_DIR}/../latproc/iod/src/)

configure_file (
  "${PROJECT_SOURCE_DIR}/ScopeConfig.h.in"
  "${PROJECT_BINARY_DIR}/ScopeConfig.h"
)

FIND_PACKAGE(Boost COMPONENTS system thread program_options date_time REQUIRED )
FIND_PACKAGE(ZeroMQ REQUIRED)

set(HEADER_FILES
    ${CLOCKWORK_DIR}/MessagingInterface.h
    ${CLOCKWORK_DIR}/SocketMonitor.h
    ${CLOCKWORK_DIR}/ConnectionManager.h
    ${CLOCKWORK_DIR}/MessageLog.h
    ${CLOCKWORK_DIR}/MessageEncoding.h
    ${CLOCKWORK_DIR}/MessageHeader.h
    ${CLOCKWORK_DIR}/regular_expressions.h
    ${CLOCKWORK_DIR}/Logger.h
    ${CLOCKWORK_DIR}/anet.h
    ${CLOCKWORK_DIR}/cJSON.h
    ${CLOCKWORK_DIR}/DebugExtra.h
    ${CLOCKWORK_DIR}/value.h
    ${CLOCKWORK_DIR}/symboltable.h
    ${CLOCKWORK_DIR}/options.h
    ${CLOCKWORK_DIR}/rate.h
    ${CLOCKWORK_DIR}/Message.h
	)

add_library (Clockwork
    ${CLOCKWORK_DIR}/MessagingInterface.cpp
    ${CLOCKWORK_DIR}/SocketMonitor.cpp
    ${CLOCKWORK_DIR}/ConnectionManager.cpp
    ${CLOCKWORK_DIR}/MessageLog.cpp
    ${CLOCKWORK_DIR}/MessageEncoding.cpp
    ${CLOCKWORK_DIR}/MessageHeader.cpp
    ${CLOCKWORK_DIR}/regular_expressions.cpp
    ${CLOCKWORK_DIR}/Logger.cpp
    ${CLOCKWORK_DIR}/anet.c
    ${CLOCKWORK_DIR}/cJSON.c
    ${CLOCKWORK_DIR}/DebugExtra.cpp
    ${CLOCKWORK_DIR}/value.cpp
    ${CLOCKWORK_DIR}/symboltable.cpp
    ${CLOCKWORK_DIR}/options.cpp
    ${CLOCKWORK_DIR}/rate.cpp
    ${CLOCKWORK_DIR}/Message.cpp
	${HEADER_FILES}
)

include_directories("/usr/local/include")
#include_directories(${ZeroMQ_INCLUDE_DIR})
include_directories("/opt/local/include")
include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CLOCKWORK_DIR}")
include_directories("${Boost_INCLUDE_DIR}")

LINK_DIRECTORIES("/usr/local/lib")
LINK_DIRECTORIES("/opt/local/lib")

add_executable (Sampler src/sampler.cpp)
target_link_libraries(Sampler Clockwork ${ZeroMQ_LIBRARY} ${Boost_LIBRARIES})

add_executable (Filter src/filter.cpp src/convert_date.cpp)
target_link_libraries(Filter Clockwork ${ZeroMQ_Library} ${Boost_LIBRARIES})

add_executable (Scope src/scope.cpp)
target_link_libraries(Filter Clockwork ${ZeroMQ_Library} ${Boost_LIBRARIES})

add_executable (convert_date src/convert_date.cpp)
set_target_properties (convert_date PROPERTIES COMPILE_DEFINITIONS "TESTING" )
target_link_libraries(convert_date ${Boost_LIBRARIES})

