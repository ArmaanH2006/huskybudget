// Fills data/huskybudget.json with 12 months of realistic sample transactions.
//
// Run from the project folder, with the server stopped:
//     .\build\seed.exe            (refuses if data already exists)
//     .\build\seed.exe --force    (replaces existing data)

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>

#include "budget/JsonFileRepository.h"
#include "budget/SampleData.h"

using namespace budget;

namespace {

// Today's date on this computer.
Date today() {
    std::time_t now = std::time(nullptr);
    std::tm local = *std::localtime(&now);
    return Date{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string path = "data/huskybudget.json";
    const bool force = (argc > 1 && std::string(argv[1]) == "--force");

    if (std::filesystem::exists(path) && !force) {
        std::cerr << path << " already exists.\n"
                  << "Run  .\\build\\seed.exe --force  to replace it with sample data.\n";
        return 1;
    }

    std::filesystem::create_directories("data");
    std::filesystem::remove(path);                 // start from an empty file

    const int kUserId = 1;
    const unsigned kSeed = 2026;                   // fixed, so every demo looks the same
    auto transactions = generateSampleData(today(), 12, kSeed, kUserId);

    JsonFileRepository repo(path);
    for (const Transaction& t : transactions) repo.addTransaction(t);

    // A few starter budgets so the Budgets section isn't empty.
    struct Starter { const char* category; std::int64_t limitCents; };
    const Starter starters[] = {
        {"Groceries", 40000}, {"Dining", 25000}, {"Shopping", 20000},
        {"Transportation", 15000}, {"Coffee", 6000}, {"Entertainment", 7500},
    };
    for (const Starter& s : starters) {
        repo.setBudget(Budget{kUserId, s.category, Money::fromCents(s.limitCents)});
    }

    std::cout << "Wrote " << transactions.size() << " transactions covering the last 12 months to "
              << path << ", plus " << std::size(starters) << " starter budgets.\n";
    return 0;
}