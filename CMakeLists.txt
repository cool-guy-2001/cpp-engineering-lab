cmake_minimum_required(VERSION 3.20)


project(
    cpp_engineering_lab
    VERSION 0.1
    LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_executable(
    cpp_lab
    main.cpp
    src/unique_fd.cpp
)


target_include_directories(
    cpp_lab
    PRIVATE
    include
)


target_compile_options(
    cpp_lab
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
