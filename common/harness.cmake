# TODO: flags should be the per-primitive, not global
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")

target_compile_features(benchmark PRIVATE cxx_std_23)
target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_LIST_DIR})