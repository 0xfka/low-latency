cmake_minimum_required(VERSION 3.20)
project(ticker-lookup LANGUAGES CXX)

include(../common/CMakeLists.txt)
add_compile_options(-mtune=znver3)
add_executable(ticker_lookup benchmark.cpp)
