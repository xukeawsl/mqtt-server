cmake_minimum_required(VERSION 3.22)

project(mqtt-server-test VERSION 3.1.1 LANGUAGES CXX)


file(GLOB_RECURSE test_sources src/*.cpp)


foreach(file ${test_sources})
    string(REGEX REPLACE "(.*/)([a-zA-Z0-9_ ]+)(\.cpp)" "\\2" test_name ${file}) 

    add_executable(${test_name}_Tests ${file})

    target_include_directories(${test_name}_Tests PUBLIC
        ${PROJECT_SOURCE_DIR}/../third-party/asio/include
    )

    find_package(GTest REQUIRED)
    target_link_libraries(${test_name}_Tests PUBLIC gtest gtest_main pthread mosquitto)

    add_test(
        NAME
        ${test_name}
        COMMAND
        ${test_name}_Tests
    )
endforeach()