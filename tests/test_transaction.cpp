#include "doctest.h"
#include "budget/Transaction.h"

using namespace budget;

TEST_CASE("transaction defaults") {
    Transaction t;
    CHECK(t.id == 0);
    CHECK(t.amount == Money());
    CHECK(t.date == Date{1970, 1, 1});
    CHECK(t.kind == TxnKind::Expense);
    CHECK(t.category.empty());
}

TEST_CASE("building a real transaction") {
    Transaction t;
    t.id       = 1;
    t.date     = *Date::parse("2026-10-03");
    t.amount   = -*Money::parse("12.50");    // spending is negative
    t.category = "Dining";
    t.merchant = "Chipotle";

    CHECK(t.amount.cents() == -1250);
    CHECK(t.amount.toString() == "-$12.50");
    CHECK(t.date.toIso() == "2026-10-03");
}