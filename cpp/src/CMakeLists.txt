cmake_minimum_required(VERSION 3.10)

PROJECT(iPerfer)
SET(CMAKE_CXX_STANDARD 23)


add_executable(iPerfer main.cpp)

target_link_libraries(iPerfer spdlog::spdlog cxxopts::cxxopts)