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

if (EXISTS "${PROJECT_SOURCE_DIR}/LocalCMakeLists.txt")
	include("${PROJECT_SOURCE_DIR}/LocalCMakeLists.txt")
endif()

configure_file (
  "${PROJECT_SOURCE_DIR}/ScopeConfig.h.in"
  "${PROJECT_BINARY_DIR}/ScopeConfig.h"
)

FIND_PACKAGE(Boost COMPONENTS thread program_options date_time context)
if (NOT Boost_FOUND)
    FIND_PACKAGE(Boost COMPONENTS thread program_options date_time REQUIRED)
endif()
FIND_PACKAGE(ZeroMQ REQUIRED)
FIND_PACKAGE(ClockworkClient REQUIRED)

include_directories("/usr/local/include")
if (EXISTS "/opt/local/include")
    include_directories("/opt/local/include")
endif()
include_directories("${PROJECT_SOURCE_DIR}/")
include_directories("${PROJECT_BINARY_DIR}")
if(ClockworkClient_INCLUDE_DIR)
    include_directories("${ClockworkClient_INCLUDE_DIR}")
endif()
# A system Boost under /usr/include can leave Boost_INCLUDE_DIR empty while
# FindBoost still reports the libraries found. CMake treats a quoted empty
# string as an include directory and aborts ("given empty-string as include
# directory"). Clockwork's list is unquoted, which is why that build passed.
if(NOT Boost_INCLUDE_DIR AND Boost_INCLUDE_DIRS)
    list(GET Boost_INCLUDE_DIRS 0 Boost_INCLUDE_DIR)
endif()
if(Boost_INCLUDE_DIR)
    include_directories("${Boost_INCLUDE_DIR}")
endif()

if (EXISTS "/usr/local/lib")
    link_directories("/usr/local/lib")
endif()
if (EXISTS "/opt/local/lib")
    link_directories("/opt/local/lib")
endif()

add_executable (Sampler src/sampler.cpp)
target_link_libraries(Sampler ${ClockworkClient_LIBRARIES} ${ZeroMQ_LIBRARY} ${Boost_LIBRARIES})

add_executable (Filter src/filter.cpp src/convert_date.cpp)
target_link_libraries(Filter ${ClockworkClient_LIBRARIES} ${ZeroMQ_LIBRARY} ${Boost_LIBRARIES})

add_executable (Scope src/scope.cpp)
target_link_libraries(Scope ${ClockworkClient_LIBRARIES} ${ZeroMQ_LIBRARY} ${Boost_LIBRARIES})

add_executable (convert_date src/convert_date.cpp)
set_target_properties (convert_date PROPERTIES COMPILE_DEFINITIONS "TESTING")
target_link_libraries(convert_date ${Boost_LIBRARIES})
