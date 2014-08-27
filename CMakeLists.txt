cmake_minimum_required (VERSION 2.6)
project (Scope)

set (Scope_VERSION_MAJOR 0)
set (Scope_VERSION_MINOR 8)

set (CMAKE_CXX_FLAGS "-g -Wall")

set (CLOCKWORK_DIR ${PROJECT_SOURCE_DIR}/../latproc/iod/)

configure_file (
  "${PROJECT_SOURCE_DIR}/ScopeConfig.h.in"
  "${PROJECT_BINARY_DIR}/ScopeConfig.h"
)

FIND_PACKAGE(Boost COMPONENTS system thread program_options REQUIRED )

add_library (Clockwork 
    ${CLOCKWORK_DIR}/MessagingInterface.cpp 
    ${CLOCKWORK_DIR}/MessageLog.cpp 
    ${CLOCKWORK_DIR}/MessageEncoding.cpp 
    ${CLOCKWORK_DIR}/regular_expressions.cpp 
    ${CLOCKWORK_DIR}/Logger.cpp
    ${CLOCKWORK_DIR}/anet.c
    ${CLOCKWORK_DIR}/cJSON.c
    ${CLOCKWORK_DIR}/DebugExtra.cpp
    ${CLOCKWORK_DIR}/value.cpp 
    ${CLOCKWORK_DIR}/symboltable.cpp
    ${CLOCKWORK_DIR}/options.cpp
    ${CLOCKWORK_DIR}//message.cpp )

include_directories("/usr/local/include")
include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CLOCKWORK_DIR}")

LINK_DIRECTORIES("/usr/local/lib")

add_executable (Sampler src/sampler.cpp)
target_link_libraries(Sampler Clockwork "zmq" ${Boost_LIBRARIES})

add_executable (Filter src/filter.cpp)
target_link_libraries(Filter Clockwork "zmq" ${Boost_LIBRARIES})

add_executable (Scope src/scope.cpp)
target_link_libraries(Filter Clockwork "zmq" ${Boost_LIBRARIES})
