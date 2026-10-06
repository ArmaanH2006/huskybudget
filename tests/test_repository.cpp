#include "doctest.h"
#include "budget/InMemoryRepository.h"

using namespace budget;

namespace {

// Small helper so each test can make a transaction in one line.
Transaction makeTxn(int userId, const char* date, const char* amount, const char* category) {
    Transaction t;
    t.userId   = userId;
    t.date     = *Date::parse(date);
    t.amount   = *Money::parse(amount);
    t.category = category;
    return t;
}

}  // namespace

TEST_CASE("addTransaction hands out increasing ids") {
    InMemoryRepository repo;
    int first  = repo.addTransaction(makeTxn(1, "2026-10-01", "-5.00", "Dining"));
    int second = repo.addTransaction(makeTxn(1, "2026-10-02", "-7.00", "Dining"));
    CHECK(first == 1);
    CHECK(second == 2);
}

TEST_CASE("transactionsInRange filters by date, inclusive at both ends") {
    InMemoryRepository repo;
    repo.addTransaction(makeTxn(1, "2026-09-30", "-1.00", "A"));   // day before: out
    repo.addTransaction(makeTxn(1, "2026-10-01", "-2.00", "B"));   // first day:  in
    repo.addTransaction(makeTxn(1, "2026-10-15", "-3.00", "C"));   // middle:     in
    repo.addTransaction(makeTxn(1, "2026-10-31", "-4.00", "D"));   // last day:   in
    repo.addTransaction(makeTxn(1, "2026-11-01", "-5.00", "E"));   // day after:  out

    auto oct = repo.transactionsInRange(1, *Date::parse("2026-10-01"), *Date::parse("2026-10-31"));
    REQUIRE(oct.size() == 3);
    CHECK(oct[0].category == "B");
    CHECK(oct[1].category == "C");
    CHECK(oct[2].category == "D");
}

TEST_CASE("transactionsInRange returns oldest first even if added out of order") {
    InMemoryRepository repo;
    repo.addTransaction(makeTxn(1, "2026-10-20", "-1.00", "late"));
    repo.addTransaction(makeTxn(1, "2026-10-05", "-1.00", "early"));

    auto all = repo.transactionsInRange(1, *Date::parse("2026-10-01"), *Date::parse("2026-10-31"));
    REQUIRE(all.size() == 2);
    CHECK(all[0].category == "early");
    CHECK(all[1].category == "late");
}

TEST_CASE("users only see their own transactions") {
    InMemoryRepository repo;
    repo.addTransaction(makeTxn(1, "2026-10-01", "-5.00", "mine"));
    repo.addTransaction(makeTxn(2, "2026-10-01", "-9.00", "theirs"));

    auto mine = repo.transactionsInRange(1, *Date::parse("2026-10-01"), *Date::parse("2026-10-31"));
    REQUIRE(mine.size() == 1);
    CHECK(mine[0].category == "mine");
}

TEST_CASE("deleteTransaction") {
    InMemoryRepository repo;
    int id = repo.addTransaction(makeTxn(1, "2026-10-01", "-5.00", "Dining"));

    CHECK_FALSE(repo.deleteTransaction(id, 2));    // wrong user: refused
    CHECK_FALSE(repo.deleteTransaction(999, 1));   // no such id
    CHECK(repo.deleteTransaction(id, 1));          // correct: deleted
    CHECK_FALSE(repo.deleteTransaction(id, 1));    // already gone

    auto left = repo.transactionsInRange(1, *Date::parse("2026-01-01"), *Date::parse("2026-12-31"));
    CHECK(left.empty());
}

TEST_CASE("setBudget adds, then updates instead of duplicating") {
    InMemoryRepository repo;
    repo.setBudget({1, "Dining", *Money::parse("200")});
    repo.setBudget({1, "Groceries", *Money::parse("400")});
    repo.setBudget({1, "Dining", *Money::parse("250")});    // raise the Dining limit

    auto budgets = repo.budgetsFor(1);
    REQUIRE(budgets.size() == 2);
    CHECK(budgets[0].category == "Dining");
    CHECK(budgets[0].monthlyLimit == *Money::parse("250"));
    CHECK(repo.budgetsFor(2).empty());
}

TEST_CASE("code can use any Repository through the interface") {
    InMemoryRepository concrete;
    Repository& repo = concrete;     // only knows the interface

    repo.addTransaction(makeTxn(1, "2026-10-01", "-5.00", "Dining"));
    CHECK(repo.transactionsInRange(1, *Date::parse("2026-10-01"), *Date::parse("2026-10-01")).size() == 1);
}