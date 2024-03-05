cmake_minimum_required(VERSION 3.22)

project(mqtt-server VERSION 3.1.1 LANGUAGES CXX)

include(cmake/settings.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wno-unused -DNDEBUG")

add_subdirectory(third-party/spdlog)

include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)
FetchContent_GetProperties(yaml-cpp)

if(NOT yaml-cpp_POPULATED)
  message(STATUS "Fetching yaml-cpp...")
  FetchContent_Populate(yaml-cpp)
  add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR})
endif()

message(STATUS "Build ${PROJECT_NAME}: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (NOT LOG_LEVEL)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
    else()
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
    endif()
else()
    if (LOG_LEVEL STREQUAL "Trace")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
    elseif(LOG_LEVEL STREQUAL "Debug")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
    elseif(LOG_LEVEL STREQUAL "Info")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
    elseif(LOG_LEVEL STREQUAL "Warn")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN)
    elseif(LOG_LEVEL STREQUAL "Error")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_ERROR)
    elseif(LOG_LEVEL STREQUAL "Critical")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_CRITICAL)
    elseif(LOG_LEVEL STREQUAL "Off")
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_OFF)
    else()
        add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
    endif()
endif()

file(GLOB_RECURSE srcs ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE hrds ${PROJECT_SOURCE_DIR}/src/*.h)

include_directories(
    third-party/asio/include
    src/
)

add_executable(${PROJECT_NAME} main.cpp ${srcs})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} PUBLIC
        pthread
        spdlog::spdlog
        yaml-cpp::yaml-cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32 wsock32 spdlog::spdlog yaml-cpp::yaml-cpp)
else()
    message(STATUS "This operating system is not supported")
endif()

#
# SSL/TLS
#
if (MQ_WITH_TLS)
    message(STATUS "Enable SSL/TLS for mqtt server")
    add_definitions(-DMQ_WITH_TLS)

    find_package(OpenSSL REQUIRED)

    target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::SSL)
endif()

#
# Valgrind
#
find_program(VALGRIND_EXECUTABLE
    NAMES "valgrind"
    PATHS "/usr/bin" "/usr/local/bin"
)

if (VALGRIND_EXECUTABLE)
    message(STATUS "Successfully find program `valgrind`")
    message(STATUS "You can use the `make valgrind` command to perform memory leak detection")
    add_custom_target(valgrind
    COMMAND
        ${VALGRIND_EXECUTABLE} --log-file=memcheck.log --leak-check=full --track-origins=yes $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT
        "Perform memory leak detection, end with `Ctrl + C`"
    )
endif()


#
# Clang-Format
#
find_program(CLANG_FORMAT_EXECUTABLE
    NAME "clang-format"
    PATHS "/usr/bin" "/usr/local/bin"
)

if (CLANG_FORMAT_EXECUTABLE)
    message(STATUS "Successfully find program `clang-format`")
    message(STATUS "You can use the `make clang-format` command to automatically format the code style")
    add_custom_target(clang-format ALL
    COMMAND
        ${CLANG_FORMAT_EXECUTABLE} --style=file -i ${hdrs};${srcs};${PROJECT_SOURCE_DIR}/main.cpp
    COMMENT
        "Automatically format the code style"
    )
endif()

#
# Unit testing setup
#
if (ENABLE_UNIT_TESTING)
    enable_testing()
    message(STATUS "Build unit tests for the project. Tests should always be found in the test folder")
    add_subdirectory(test)
endif()