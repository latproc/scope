cmake_minimum_required (VERSION 3.10)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
project (Scope)

set (Scope_VERSION_MAJOR 0)
set (Scope_VERSION_MINOR 8)

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")

set (CMAKE_EXPORT_COMPILE_COMMANDS true)

set (CMAKE_CXX_FLAGS "-g -Wall --std=c++11")
add_compile_options(-Wno-deprecated-declarations)

# Client library sources live in the clockwork submodule. We build only
# cw_client (not iod/cw/ethercat or the rest of clockwork). Keep this list
# aligned with clockwork/iod/CMakeLists.txt target cw_client.
set (CLOCKWORK_DIR ${PROJECT_SOURCE_DIR}/clockwork/iod/src)

if (EXISTS "${PROJECT_SOURCE_DIR}/LocalCMakeLists.txt")
	include("${PROJECT_SOURCE_DIR}/LocalCMakeLists.txt")
endif()

configure_file (
  "${PROJECT_SOURCE_DIR}/ScopeConfig.h.in"
  "${PROJECT_BINARY_DIR}/ScopeConfig.h"
)

# context is required by process.cpp in cw_client
FIND_PACKAGE(Boost COMPONENTS system thread program_options date_time context REQUIRED)
FIND_PACKAGE(ZeroMQ REQUIRED)

set(CW_CLIENT_HEADERS
    ${CLOCKWORK_DIR}/cw_client.h
    ${CLOCKWORK_DIR}/MessagingInterface.h
    ${CLOCKWORK_DIR}/SocketMonitor.h
    ${CLOCKWORK_DIR}/ConnectionManager.h
    ${CLOCKWORK_DIR}/MessageLog.h
    ${CLOCKWORK_DIR}/MessageEncoding.h
    ${CLOCKWORK_DIR}/MessageHeader.h
    ${CLOCKWORK_DIR}/regular_expressions.h
    ${CLOCKWORK_DIR}/Logger.h
    ${CLOCKWORK_DIR}/anet.h
    ${CLOCKWORK_DIR}/clock.h
    ${CLOCKWORK_DIR}/cJSON.h
    ${CLOCKWORK_DIR}/DebugExtra.h
    ${CLOCKWORK_DIR}/value.h
    ${CLOCKWORK_DIR}/urlencode.h
    ${CLOCKWORK_DIR}/symboltable.h
    ${CLOCKWORK_DIR}/rate.h
    ${CLOCKWORK_DIR}/watchdog.h
    ${CLOCKWORK_DIR}/Message.h
    ${CLOCKWORK_DIR}/Parameter.h
    ${CLOCKWORK_DIR}/Receiver.h
    ${CLOCKWORK_DIR}/json_expr_parser.h
    ${CLOCKWORK_DIR}/json_expression.h
    ${CLOCKWORK_DIR}/process.h
    ${CLOCKWORK_DIR}/ThreadSafeQueue.h
    ${CLOCKWORK_DIR}/ThreadSafeList.h
)

# Static client library only — same sources as clockwork's cw_client target
add_library (cw_client
    ${CLOCKWORK_DIR}/MessagingInterface.cpp
    ${CLOCKWORK_DIR}/MessageLog.cpp
    ${CLOCKWORK_DIR}/MessageEncoding.cpp
    ${CLOCKWORK_DIR}/MessageHeader.cpp
    ${CLOCKWORK_DIR}/regular_expressions.cpp
    ${CLOCKWORK_DIR}/CommandManager.cpp
    ${CLOCKWORK_DIR}/SocketMonitor.cpp
    ${CLOCKWORK_DIR}/ConnectionManager.cpp
    ${CLOCKWORK_DIR}/Logger.cpp
    ${CLOCKWORK_DIR}/anet.c
    ${CLOCKWORK_DIR}/clock.cpp
    ${CLOCKWORK_DIR}/cJSON.c
    ${CLOCKWORK_DIR}/DebugExtra.cpp
    ${CLOCKWORK_DIR}/value.cpp
    ${CLOCKWORK_DIR}/urlencode.cpp
    ${CLOCKWORK_DIR}/symboltable.cpp
    ${CLOCKWORK_DIR}/rate.cpp
    ${CLOCKWORK_DIR}/Message.cpp
    ${CLOCKWORK_DIR}/Parameter.cpp
    ${CLOCKWORK_DIR}/watchdog.cpp
    ${CLOCKWORK_DIR}/json_expr_parser.cpp
    ${CLOCKWORK_DIR}/json_expression.cpp
    ${CLOCKWORK_DIR}/process.cpp
    ${CW_CLIENT_HEADERS}
)

# Keep the historical target name as an alias for any local overrides
add_library (Clockwork ALIAS cw_client)

include_directories("/usr/local/include")
if (EXISTS "/opt/local/include")
    include_directories("/opt/local/include")
endif()
include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CLOCKWORK_DIR}")
include_directories("${Boost_INCLUDE_DIR}")

if (EXISTS "/usr/local/lib")
    link_directories("/usr/local/lib")
endif()
if (EXISTS "/opt/local/lib")
    link_directories("/opt/local/lib")
endif()

add_executable (Sampler src/sampler.cpp)
target_link_libraries(Sampler cw_client ${ZeroMQ_LIBRARY} ${Boost_LIBRARIES})

add_executable (Filter src/filter.cpp src/convert_date.cpp)
target_link_libraries(Filter cw_client ${ZeroMQ_LIBRARY} ${Boost_LIBRARIES})

add_executable (Scope src/scope.cpp)

add_executable (convert_date src/convert_date.cpp)
set_target_properties (convert_date PROPERTIES COMPILE_DEFINITIONS "TESTING")
target_link_libraries(convert_date ${Boost_LIBRARIES})
