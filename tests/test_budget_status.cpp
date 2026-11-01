#include "doctest.h"
#include "budget/BudgetStatus.h"

using namespace budget;

namespace {

const YearMonth kOctober{2026, 10};

Transaction spend(const char* date, const char* amount, const char* category) {
    Transaction t;
    t.date     = *Date::parse(date);
    t.amount   = -*Money::parse(amount);       // expenses are stored negative
    t.category = category;
    t.kind     = TxnKind::Expense;
    return t;
}

Budget makeBudget(const char* category, const char* limit) {
    Budget b;
    b.userId       = 1;
    b.category     = category;
    b.monthlyLimit = *Money::parse(limit);
    return b;
}

Money dollars(const char* text) { return *Money::parse(text); }

}  // namespace

TEST_CASE("spent only counts expenses in that budget's category") {
    std::vector<Transaction> txns = {
        spend("2026-10-02", "5.25",  "Coffee"),
        spend("2026-10-05", "4.75",  "Coffee"),
        spend("2026-10-05", "40.00", "Dining"),
    };
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, txns, kOctober, Date{2026, 10, 31});
    REQUIRE(result.size() == 1);
    CHECK(result[0].category == "Coffee");
    CHECK(result[0].spent == dollars("10.00"));
    CHECK(result[0].remaining == dollars("50.00"));
    CHECK(result[0].percentUsed == 16);              // 10 / 60 = 16.67%, rounded down
}

TEST_CASE("income and transfers are not spending") {
    Transaction refund = spend("2026-10-03", "20.00", "Shopping");
    refund.kind = TxnKind::Income;
    refund.amount = dollars("20.00");
    Transaction moved = spend("2026-10-04", "100.00", "Shopping");
    moved.kind = TxnKind::Transfer;

    auto result = evaluateBudgets({makeBudget("Shopping", "200")}, {refund, moved}, kOctober, Date{2026, 10, 31});
    CHECK(result[0].spent == Money());
}

TEST_CASE("transactions outside the month are ignored") {
    std::vector<Transaction> txns = {
        spend("2026-09-30", "50.00", "Dining"),
        spend("2026-10-01", "10.00", "Dining"),
        spend("2026-10-31", "10.00", "Dining"),
        spend("2026-11-01", "50.00", "Dining"),
    };
    auto result = evaluateBudgets({makeBudget("Dining", "250")}, txns, kOctober, Date{2026, 11, 15});
    CHECK(result[0].spent == dollars("20.00"));       // only Oct 1 and Oct 31
}

TEST_CASE("a budget with no spending shows zero") {
    auto result = evaluateBudgets({makeBudget("Entertainment", "75")}, {}, kOctober, Date{2026, 10, 20});
    CHECK(result[0].spent == Money());
    CHECK(result[0].remaining == dollars("75.00"));
    CHECK(result[0].percentUsed == 0);
    CHECK(result[0].projected == Money());
    CHECK(result[0].health == BudgetHealth::OnTrack);
}

TEST_CASE("spending more than the limit is Over") {
    std::vector<Transaction> txns = {spend("2026-10-03", "65.00", "Coffee")};
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, txns, kOctober, Date{2026, 10, 20});
    CHECK(result[0].health == BudgetHealth::Over);
    CHECK(result[0].remaining == dollars("-5.00"));
    CHECK(result[0].percentUsed == 108);
}

TEST_CASE("spending exactly the limit in a finished month is not Over") {
    std::vector<Transaction> txns = {spend("2026-10-03", "60.00", "Coffee")};
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, txns, kOctober, Date{2026, 11, 2});
    CHECK(result[0].health == BudgetHealth::OnTrack);
    CHECK(result[0].remaining == Money());
    CHECK(result[0].percentUsed == 100);
}

TEST_CASE("the Coffee example: only 26% used, but at risk") {
    // $15.68 in the first 8 days of a 31-day month.
    // Pace: 15.68 / 8 * 31 = 60.76, which is over the $60 limit.
    std::vector<Transaction> txns = {
        spend("2026-10-02", "5.25", "Coffee"),
        spend("2026-10-05", "4.95", "Coffee"),
        spend("2026-10-08", "5.48", "Coffee"),
    };
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, txns, kOctober, Date{2026, 10, 8});
    CHECK(result[0].spent == dollars("15.68"));
    CHECK(result[0].percentUsed == 26);
    CHECK(result[0].projected == dollars("60.76"));
    CHECK_FALSE(result[0].tooEarly);
    CHECK(result[0].health == BudgetHealth::AtRisk);
}

TEST_CASE("a slow pace stays On track") {
    // $20 in 10 days of a 31-day month -> about $62 for the month, under $100.
    std::vector<Transaction> txns = {spend("2026-10-04", "20.00", "Groceries")};
    auto result = evaluateBudgets({makeBudget("Groceries", "100")}, txns, kOctober, Date{2026, 10, 10});
    CHECK(result[0].projected == dollars("62.00"));
    CHECK(result[0].health == BudgetHealth::OnTrack);
}

TEST_CASE("too early in the month to judge the pace") {
    // $30 in 2 days would be a pace of $465, but one purchase isn't a trend.
    std::vector<Transaction> txns = {spend("2026-10-02", "30.00", "Dining")};
    auto result = evaluateBudgets({makeBudget("Dining", "250")}, txns, kOctober, Date{2026, 10, 2});
    CHECK(result[0].tooEarly);
    CHECK(result[0].projected == dollars("30.00"));
    CHECK(result[0].health == BudgetHealth::OnTrack);

    // Day 7 is the first day the pace counts.
    auto day6 = evaluateBudgets({makeBudget("Dining", "250")}, txns, kOctober, Date{2026, 10, 6});
    auto day7 = evaluateBudgets({makeBudget("Dining", "250")}, txns, kOctober, Date{2026, 10, 7});
    CHECK(day6[0].tooEarly);
    CHECK_FALSE(day7[0].tooEarly);
}

TEST_CASE("too early still says Over when you're already over") {
    std::vector<Transaction> txns = {spend("2026-10-01", "80.00", "Coffee")};
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, txns, kOctober, Date{2026, 10, 1});
    CHECK(result[0].tooEarly);
    CHECK(result[0].health == BudgetHealth::Over);
}

TEST_CASE("a past month is finished: the guess is just what was spent") {
    std::vector<Transaction> txns = {spend("2026-10-05", "40.00", "Coffee")};
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, txns, kOctober, Date{2027, 1, 15});
    CHECK(result[0].projected == dollars("40.00"));
    CHECK_FALSE(result[0].tooEarly);
}

TEST_CASE("a future month hasn't started yet") {
    auto result = evaluateBudgets({makeBudget("Coffee", "60")}, {}, kOctober, Date{2026, 9, 20});
    CHECK(result[0].tooEarly);
    CHECK(result[0].health == BudgetHealth::OnTrack);
}

TEST_CASE("February uses its real length") {
    // $28 in 14 days of a 28-day February -> $56 for the month.
    std::vector<Transaction> txns = {spend("2026-02-03", "28.00", "Coffee")};
    auto result = evaluateBudgets({makeBudget("Coffee", "50")}, txns, YearMonth{2026, 2}, Date{2026, 2, 14});
    CHECK(result[0].projected == dollars("56.00"));
    CHECK(result[0].health == BudgetHealth::AtRisk);
}

TEST_CASE("budgets come back most-used first") {
    std::vector<Transaction> txns = {
        spend("2026-10-03", "30.00",  "Coffee"),      // 50%
        spend("2026-10-03", "200.00", "Groceries"),   // 50%
        spend("2026-10-03", "225.00", "Dining"),      // 90%
    };
    auto result = evaluateBudgets(
        {makeBudget("Coffee", "60"), makeBudget("Dining", "250"), makeBudget("Groceries", "400"), makeBudget("Shopping", "200")},
        txns, kOctober, Date{2026, 10, 31});
    REQUIRE(result.size() == 4);
    CHECK(result[0].category == "Dining");
    CHECK(result[1].category == "Coffee");          // tie at 50%: alphabetical
    CHECK(result[2].category == "Groceries");
    CHECK(result[3].category == "Shopping");        // 0%
}

TEST_CASE("health names for the web page") {
    CHECK(std::string(budgetHealthName(BudgetHealth::OnTrack)) == "on-track");
    CHECK(std::string(budgetHealthName(BudgetHealth::AtRisk))  == "at-risk");
    CHECK(std::string(budgetHealthName(BudgetHealth::Over))    == "over");
}