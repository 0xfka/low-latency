cmake_minimum_required(VERSION 3.20)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
    project(low-latency-common LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-march=native)
# Default, add the ones that matters on specific code inside
# file / dir ckmake
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -flto")
set(CMAKE_CXX_FLAGS_SANITIZE
    "-O0 -g -fno-omit-frame-pointer -fsanitize=address,undefined")

# So we can disable at CI
option(COMMON_BUILD_BENCHMARKS "Build benchmarks" ON)
if(COMMON_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    include(FetchContent)
    FetchContent_Declare(fastrange
        GIT_REPOSITORY https://github.com/lemire/fastrange.git
        GIT_TAG master)
    FetchContent_MakeAvailable(fastrange)
    link_libraries(benchmark::benchmark fastrange::fastrange)
endif()


include_directories("${CMAKE_CURRENT_LIST_DIR}")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
    enable_testing()
    add_executable(harness_test harness_test.cpp)
    add_test(NAME harness_test COMMAND harness_test)
    endif()
