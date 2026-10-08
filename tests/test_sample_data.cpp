#include "doctest.h"
#include "budget/SampleData.h"
#include "budget/Summary.h"

#include <set>

using namespace budget;

TEST_CASE("sample data covers exactly the requested months, nothing in the future") {
    Date last{2026, 10, 8};
    auto txns = generateSampleData(last, 12, 42, 1);
    REQUIRE_FALSE(txns.empty());

    Date earliest = txns[0].date, latest = txns[0].date;
    for (const auto& t : txns) {
        if (t.date < earliest) earliest = t.date;
        if (latest < t.date)   latest = t.date;
    }
    CHECK_FALSE(earliest < Date{2025, 11, 1});     // starts November 2025
    CHECK_FALSE(last < latest);                     // nothing after Oct 8, 2026

    std::set<std::string> months;
    for (const auto& t : txns) months.insert(t.date.toIso().substr(0, 7));
    CHECK(months.size() == 12);
}

TEST_CASE("the same seed gives the same data, a different seed doesn't") {
    Date last{2026, 10, 31};
    auto a = generateSampleData(last, 6, 7, 1);
    auto b = generateSampleData(last, 6, 7, 1);
    auto c = generateSampleData(last, 6, 8, 1);

    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].date == b[i].date);
        CHECK(a[i].amount == b[i].amount);
        CHECK(a[i].merchant == b[i].merchant);
    }

    bool anyDifferent = a.size() != c.size();
    for (std::size_t i = 0; !anyDifferent && i < a.size(); ++i)
        anyDifferent = !(a[i].amount == c[i].amount);
    CHECK(anyDifferent);
}

TEST_CASE("every transaction is valid and signed correctly") {
    auto txns = generateSampleData(Date{2026, 10, 31}, 12, 1, 5);
    for (const auto& t : txns) {
        CHECK(t.userId == 5);
        CHECK(Date::parse(t.date.toIso()).has_value());       // a real calendar date
        CHECK_FALSE(t.category.empty());
        if (t.kind == TxnKind::Income) CHECK(Money() < t.amount);
        else                           CHECK(t.amount < Money());
    }
}

TEST_CASE("a full sample month looks like a real budget") {
    auto txns = generateSampleData(Date{2026, 9, 30}, 1, 3, 1);   // just September
    MonthSummary s = summarize(txns);

    CHECK(s.income.toString() == "$2,400.00");                   // two paychecks
    CHECK(Money::fromCents(150000) < s.spending);                 // rent alone is $1,100
    CHECK(s.spending < s.income);                                 // a sensible month
    CHECK(s.spendingByCategory.front().category == "Rent");       // the biggest expense
}

TEST_CASE("the last month stops at lastDay") {
    auto txns = generateSampleData(Date{2026, 10, 3}, 1, 9, 1);
    for (const auto& t : txns) CHECK_FALSE(Date{2026, 10, 3} < t.date);
}