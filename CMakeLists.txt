cmake_minimum_required(VERSION 3.16)
project(huskybudget LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(huskybudget apps/server_main.cpp)