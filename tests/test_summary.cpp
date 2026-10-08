#include "doctest.h"
#include "budget/Summary.h"

using namespace budget;

namespace {

Transaction txn(const char* amount, const char* category, TxnKind kind = TxnKind::Expense) {
    Transaction t;
    t.amount   = *Money::parse(amount);
    t.category = category;
    t.kind     = kind;
    return t;
}

}  // namespace

TEST_CASE("YearMonth parse and range") {
    auto oct = YearMonth::parse("2026-10");
    REQUIRE(oct.has_value());
    CHECK(oct->toString() == "2026-10");
    CHECK(oct->firstDay() == Date{2026, 10, 1});
    CHECK(oct->lastDay()  == Date{2026, 10, 31});

    CHECK(YearMonth::parse("2024-02")->lastDay() == Date{2024, 2, 29});   // leap year
    CHECK(YearMonth::parse("2026-02")->lastDay() == Date{2026, 2, 28});

    for (const char* bad : {"", "2026", "2026-13", "2026-1", "2026/10", "abcd-ef", "2026-10-01"}) {
        INFO("input: ", bad);
        CHECK_FALSE(YearMonth::parse(bad).has_value());
    }
}

TEST_CASE("summary of an empty month is all zeros") {
    MonthSummary s = summarize({});
    CHECK(s.income == Money());
    CHECK(s.spending == Money());
    CHECK(s.net == Money());
    CHECK(s.transactionCount == 0);
    CHECK(s.spendingByCategory.empty());
}

TEST_CASE("income, spending and net") {
    MonthSummary s = summarize({
        txn("2400.00",  "Salary", TxnKind::Income),
        txn("-1100.00", "Rent"),
        txn("-64.18",   "Groceries"),
        txn("-12.50",   "Dining"),
    });
    CHECK(s.income.toString()   == "$2,400.00");
    CHECK(s.spending.toString() == "$1,176.68");       // shown as a positive number
    CHECK(s.net.toString()      == "$1,223.32");
    CHECK(s.transactionCount == 4);
}

TEST_CASE("net goes negative when you overspend") {
    MonthSummary s = summarize({
        txn("500.00", "Salary", TxnKind::Income),
        txn("-800.00", "Rent"),
    });
    CHECK(s.net.toString() == "-$300.00");
}

TEST_CASE("spending by category adds up and is sorted biggest first") {
    MonthSummary s = summarize({
        txn("-12.50", "Dining"),
        txn("-64.18", "Groceries"),
        txn("-8.00",  "Dining"),
        txn("-20.00", "Groceries"),
        txn("-5.25",  "Coffee"),
        txn("100.00", "Gift", TxnKind::Income),        // income: not a spending category
    });
    REQUIRE(s.spendingByCategory.size() == 3);
    CHECK(s.spendingByCategory[0].category == "Groceries");
    CHECK(s.spendingByCategory[0].total.toString() == "$84.18");
    CHECK(s.spendingByCategory[1].category == "Dining");
    CHECK(s.spendingByCategory[1].total.toString() == "$20.50");
    CHECK(s.spendingByCategory[2].category == "Coffee");

    // The categories must add up to the total spending, to the cent.
    Money sum;
    for (const auto& c : s.spendingByCategory) sum = sum + c.total;
    CHECK(sum == s.spending);
}

TEST_CASE("transfers are neither income nor spending") {
    MonthSummary s = summarize({
        txn("-200.00", "Savings", TxnKind::Transfer),
        txn("-10.00",  "Dining"),
    });
    CHECK(s.spending.toString() == "$10.00");
    CHECK(s.transactionCount == 1);
    CHECK(s.spendingByCategory.size() == 1);
}

TEST_CASE("TxnKind text round-trip") {
    for (TxnKind k : {TxnKind::Expense, TxnKind::Income, TxnKind::Transfer}) {
        CHECK(parseTxnKind(txnKindName(k)) == k);
    }
    CHECK_FALSE(parseTxnKind("refund").has_value());
}