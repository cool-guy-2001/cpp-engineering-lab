cmake_minimum_required(VERSION 3.20)

project(cpp_engineering_lab)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_executable(
    cpp_lab
    main.cpp
)


target_compile_options(
    cpp_lab
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
