cmake_minimum_required(VERSION 3.16)
project(huskybudget LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(budget_core src/Money.cpp src/Date.cpp)
target_include_directories(budget_core PUBLIC include third_party)

add_executable(huskybudget apps/server_main.cpp)
target_link_libraries(huskybudget PRIVATE budget_core)

enable_testing()
add_executable(tests tests/test_money.cpp tests/test_date.cpp tests/test_transaction.cpp)
target_link_libraries(tests PRIVATE budget_core)
add_test(NAME unit COMMAND tests)