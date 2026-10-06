#include "doctest.h"
#include "budget/JsonFileRepository.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace budget;

namespace {

// A fresh file path in the system temp folder, deleted before and after use.
struct TempFile {
    std::string path;
    explicit TempFile(const char* name)
        : path((std::filesystem::temp_directory_path() / name).string()) {
        std::remove(path.c_str());
    }
    ~TempFile() { std::remove(path.c_str()); }
};

Transaction sample(const char* date, const char* amount, const char* category) {
    Transaction t;
    t.userId   = 1;
    t.date     = *Date::parse(date);
    t.amount   = *Money::parse(amount);
    t.category = category;
    t.merchant = "Somewhere";
    return t;
}

const Date kStart = *Date::parse("2026-01-01");
const Date kEnd   = *Date::parse("2026-12-31");

}  // namespace

TEST_CASE("missing file means an empty repository") {
    TempFile f("hb_test_missing.json");
    JsonFileRepository repo(f.path);
    CHECK(repo.transactionsInRange(1, kStart, kEnd).empty());
    CHECK(repo.budgetsFor(1).empty());
}

TEST_CASE("data survives closing and reopening") {
    TempFile f("hb_test_roundtrip.json");
    {
        JsonFileRepository repo(f.path);
        Transaction t = sample("2026-10-03", "-12.50", "Dining");
        t.note = "lunch";
        t.kind = TxnKind::Expense;
        repo.addTransaction(t);

        Transaction pay = sample("2026-10-01", "1500.00", "Salary");
        pay.kind = TxnKind::Income;
        repo.addTransaction(pay);

        repo.setBudget({1, "Dining", *Money::parse("200")});
    }   // repo is destroyed here, like closing the app

    JsonFileRepository reopened(f.path);
    auto txns = reopened.transactionsInRange(1, kStart, kEnd);
    REQUIRE(txns.size() == 2);

    CHECK(txns[0].category == "Salary");                  // oldest first
    CHECK(txns[0].kind == TxnKind::Income);
    CHECK(txns[0].amount.toString() == "$1,500.00");

    CHECK(txns[1].category == "Dining");
    CHECK(txns[1].amount.cents() == -1250);                // exact, no rounding
    CHECK(txns[1].date.toIso() == "2026-10-03");
    CHECK(txns[1].note == "lunch");
    CHECK(txns[1].merchant == "Somewhere");

    auto budgets = reopened.budgetsFor(1);
    REQUIRE(budgets.size() == 1);
    CHECK(budgets[0].monthlyLimit.toString() == "$200.00");
}

TEST_CASE("ids keep counting up after reopening") {
    TempFile f("hb_test_ids.json");
    {
        JsonFileRepository repo(f.path);
        repo.addTransaction(sample("2026-10-01", "-1", "A"));   // id 1
        repo.addTransaction(sample("2026-10-02", "-1", "B"));   // id 2
    }
    JsonFileRepository reopened(f.path);
    int next = reopened.addTransaction(sample("2026-10-03", "-1", "C"));
    CHECK(next == 3);                                         // not 1 again
}

TEST_CASE("deletes are saved too") {
    TempFile f("hb_test_delete.json");
    int id = 0;
    {
        JsonFileRepository repo(f.path);
        id = repo.addTransaction(sample("2026-10-01", "-5", "Dining"));
        repo.addTransaction(sample("2026-10-02", "-6", "Groceries"));
        CHECK(repo.deleteTransaction(id, 1));
    }
    JsonFileRepository reopened(f.path);
    auto left = reopened.transactionsInRange(1, kStart, kEnd);
    REQUIRE(left.size() == 1);
    CHECK(left[0].category == "Groceries");
}

TEST_CASE("a corrupted file is an error, not silently empty") {
    TempFile f("hb_test_corrupt.json");
    {
        std::ofstream out(f.path);
        out << "{ this is not json";
    }
    CHECK_THROWS_AS(JsonFileRepository{f.path}, std::runtime_error);
}

TEST_CASE("a file with a bad date is an error") {
    TempFile f("hb_test_baddate.json");
    {
        std::ofstream out(f.path);
        out << R"({"nextId": 2, "budgets": [], "transactions": [
                  {"id": 1, "userId": 1, "date": "2026-02-30", "cents": -100,
                   "category": "X"}]})";
    }
    CHECK_THROWS_AS(JsonFileRepository{f.path}, std::runtime_error);
}

TEST_CASE("JsonFileRepository works through the Repository interface") {
    TempFile f("hb_test_iface.json");
    JsonFileRepository concrete(f.path);
    Repository& repo = concrete;           // same interface as InMemoryRepository
    repo.addTransaction(sample("2026-10-01", "-5", "Dining"));
    CHECK(repo.transactionsInRange(1, kStart, kEnd).size() == 1);
}