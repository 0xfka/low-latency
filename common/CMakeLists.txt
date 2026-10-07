cmake_minimum_required(VERSION 3.20)
project(harness-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_executable(harness_test harness_test.cpp)

add_test(NAME harness_test COMMAND harness_test)
