cmake_minimum_required(VERSION 3.16)
project(huskybudget LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(budget_core
    src/Money.cpp
    src/Date.cpp
    src/InMemoryRepository.cpp
    src/JsonFileRepository.cpp
    src/Summary.cpp
    src/SampleData.cpp
    src/CsvImporter.cpp
    src/BudgetStatus.cpp
)
target_include_directories(budget_core PUBLIC include third_party)

add_executable(huskybudget apps/server_main.cpp)
target_link_libraries(huskybudget PRIVATE budget_core Threads::Threads)
if(WIN32)
    # The HTTP library needs Windows networking (Winsock) and Windows 10+.
    target_link_libraries(huskybudget PRIVATE ws2_32)
    target_compile_definitions(huskybudget PRIVATE _WIN32_WINNT=0x0A00)
endif()

add_executable(seed apps/seed_main.cpp)
target_link_libraries(seed PRIVATE budget_core)

enable_testing()
add_executable(tests
    tests/test_money.cpp
    tests/test_date.cpp
    tests/test_transaction.cpp
    tests/test_repository.cpp
    tests/test_json_repository.cpp
    tests/test_summary.cpp
    tests/test_sample_data.cpp
    tests/test_csv_importer.cpp
    tests/test_budget_status.cpp
)
target_link_libraries(tests PRIVATE budget_core)
add_test(NAME unit COMMAND tests)