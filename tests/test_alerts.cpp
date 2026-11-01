#include "doctest.h"
#include "budget/Alerts.h"

using namespace budget;

namespace {

const YearMonth kOctober{2026, 10};

// Builds a history oldest-first and gives each transaction an id, like the repository does.
struct History {
    std::vector<Transaction> txns;

    Transaction& spend(Date date, const char* amount, const char* category, const char* merchant) {
        Transaction t;
        t.id       = static_cast<int>(txns.size()) + 1;
        t.date     = date;
        t.amount   = -*Money::parse(amount);       // expenses are stored negative
        t.category = category;
        t.merchant = merchant;
        t.kind     = TxnKind::Expense;
        txns.push_back(t);
        return txns.back();
    }
};

// `n` days after Aug 1, 2026. dayAfterAug1(0) is Aug 1, dayAfterAug1(61) is Oct 1.
Date dayAfterAug1(int n) {
    Date d{2026, 8, 1};
    for (int i = 0; i < n; ++i) {
        if (++d.day > Date::daysInMonth(d.year, d.month)) {
            d.day = 1;
            if (++d.month > 12) { d.month = 1; ++d.year; }
        }
    }
    return d;
}

Date oct(int day) { return Date{2026, 10, day}; }

// Two months of quiet history: one $20 grocery trip every other day at the same store.
History quietHistory() {
    History h;
    for (int n = 0; n < 60; n += 2) h.spend(dayAfterAug1(n), "20.00", "Groceries", "Aldi");
    return h;
}

}  // namespace

TEST_CASE("a quiet month has no alerts") {
    History h = quietHistory();
    h.spend(oct(3), "22.00", "Groceries", "Aldi");
    h.spend(oct(9), "18.50", "Groceries", "Aldi");
    CHECK(findAlerts(h.txns, kOctober).empty());
}

TEST_CASE("same store, same amount, same day: flags the second charge") {
    History h = quietHistory();
    h.spend(oct(10), "19.02", "Dining", "Chipotle");
    int secondId = h.spend(oct(10), "19.02", "Dining", "Chipotle").id;

    auto alerts = findAlerts(h.txns, kOctober);
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0].kind == AlertKind::DuplicateCharge);
    CHECK(alerts[0].transactionId == secondId);
    CHECK(alerts[0].amount == *Money::parse("19.02"));
    CHECK(alerts[0].reason.find("twice") != std::string::npos);
}

TEST_CASE("not a double charge: different amount, different day, or no store name") {
    History h = quietHistory();
    h.spend(oct(10), "19.02", "Dining", "Chipotle");
    h.spend(oct(10), "12.00", "Dining", "Chipotle");    // different amount
    h.spend(oct(11), "19.02", "Dining", "Chipotle");    // different day
    h.spend(oct(12), "5.00",  "Dining", "");
    h.spend(oct(12), "5.00",  "Dining", "");            // no merchant: can't tell
    CHECK(findAlerts(h.txns, kOctober).empty());
}

TEST_CASE("a charge far above normal for its category") {
    History h = quietHistory();
    int bigId = h.spend(oct(15), "95.00", "Groceries", "Aldi").id;   // usually $20

    auto alerts = findAlerts(h.txns, kOctober);
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0].kind == AlertKind::UnusualAmount);
    CHECK(alerts[0].transactionId == bigId);
    CHECK(alerts[0].reason == "$95.00 is about 4.8 times your usual Groceries charge (average $20.00)");
}

TEST_CASE("normal ups and downs are not unusual") {
    History h;
    const char* amounts[] = {"30.00", "55.00", "42.00", "61.00", "38.00", "47.00", "52.00", "35.00"};
    int n = 0;
    for (const char* a : amounts) h.spend(dayAfterAug1(n += 7), a, "Groceries", "Aldi");
    h.spend(oct(20), "64.00", "Groceries", "Aldi");     // the biggest yet, but within normal range
    CHECK(findAlerts(h.txns, kOctober).empty());
}

TEST_CASE("unusual needs at least 5 earlier charges to know what normal is") {
    History h;
    for (int n = 0; n < 4; ++n) h.spend(dayAfterAug1(n * 10), "20.00", "Coffee", "Dunkin");
    h.spend(oct(5), "90.00", "Coffee", "Dunkin");
    CHECK(findAlerts(h.txns, kOctober).empty());
}

TEST_CASE("when every earlier charge was identical, 1.5x is enough") {
    History h;
    for (int month = 5; month <= 9; ++month) h.spend(Date{2026, month, 1}, "1100.00", "Rent", "Landlord");

    History same = h;
    same.spend(oct(1), "1100.00", "Rent", "Landlord");
    CHECK(findAlerts(same.txns, kOctober).empty());

    History raised = h;
    raised.spend(oct(1), "1650.00", "Rent", "Landlord");
    auto alerts = findAlerts(raised.txns, kOctober);
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0].kind == AlertKind::UnusualAmount);
}

TEST_CASE("a big charge at a store you've never used") {
    History h = quietHistory();
    h.spend(oct(28), "249.00", "Electronics", "Best Buy");    // new category too, so not "unusual"

    auto alerts = findAlerts(h.txns, kOctober);
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0].kind == AlertKind::NewMerchantLarge);
    CHECK(alerts[0].reason == "First time at Best Buy, and it's a big charge ($249.00)");
}

TEST_CASE("new store rules: small charges and brand-new users don't count") {
    History small = quietHistory();
    small.spend(oct(28), "150.00", "Electronics", "Best Buy");   // under $200
    CHECK(findAlerts(small.txns, kOctober).empty());

    History brandNew;                                             // history starts Oct 1
    brandNew.spend(oct(1), "20.00", "Groceries", "Aldi");
    brandNew.spend(oct(5), "300.00", "Electronics", "Best Buy");
    CHECK(findAlerts(brandNew.txns, kOctober).empty());
}

TEST_CASE("a day with far more purchases than usual") {
    History h = quietHistory();                                   // about 0.4 purchases a day
    const char* amounts[] = {"8.00", "9.00", "10.00", "11.00", "12.00"};   // all different, so no double charges
    for (const char* a : amounts) h.spend(oct(17), a, "Shopping", "Target");

    auto alerts = findAlerts(h.txns, kOctober);
    REQUIRE(alerts.size() == 1);
    CHECK(alerts[0].kind == AlertKind::SpendingBurst);
    CHECK(alerts[0].date == oct(17));
    CHECK(alerts[0].transactionId == 0);                          // about the whole day
    CHECK(alerts[0].amount == *Money::parse("50.00"));
    CHECK(alerts[0].reason == "5 purchases in one day, adding up to $50.00. You usually make about 0.4 a day.");
}

TEST_CASE("4 purchases in a day is not a burst") {
    History h = quietHistory();
    const char* amounts[] = {"8.00", "9.00", "10.00", "11.00"};
    for (const char* a : amounts) h.spend(oct(17), a, "Shopping", "Target");
    CHECK(findAlerts(h.txns, kOctober).empty());
}

TEST_CASE("each charge gets at most one alert") {
    // A huge charge, then the same huge charge again: the first is unusual,
    // the second is a double charge (not also "unusual").
    History h = quietHistory();
    h.spend(oct(12), "95.00", "Groceries", "Aldi");
    h.spend(oct(12), "95.00", "Groceries", "Aldi");

    auto alerts = findAlerts(h.txns, kOctober);
    REQUIRE(alerts.size() == 2);
    CHECK(alerts[0].kind == AlertKind::UnusualAmount);
    CHECK(alerts[1].kind == AlertKind::DuplicateCharge);
}

TEST_CASE("only charges in the month, never income, newest first") {
    History h = quietHistory();
    h.spend(Date{2026, 9, 29}, "95.00", "Groceries", "Aldi");     // September: not this month
    h.spend(oct(5),  "95.00", "Groceries", "Aldi");
    h.spend(oct(25), "99.00", "Groceries", "Aldi");

    Transaction paycheck;
    paycheck.date   = oct(15);
    paycheck.amount = *Money::parse("5000.00");                   // huge, but it's income
    paycheck.category = "Groceries";
    paycheck.kind   = TxnKind::Income;
    h.txns.push_back(paycheck);

    auto alerts = findAlerts(h.txns, kOctober);
    REQUIRE(alerts.size() == 2);
    CHECK(alerts[0].date == oct(25));
    CHECK(alerts[1].date == oct(5));
}

TEST_CASE("alert kind names for the web page") {
    CHECK(std::string(alertKindName(AlertKind::UnusualAmount))    == "unusual-amount");
    CHECK(std::string(alertKindName(AlertKind::DuplicateCharge))  == "duplicate");
    CHECK(std::string(alertKindName(AlertKind::NewMerchantLarge)) == "new-merchant");
    CHECK(std::string(alertKindName(AlertKind::SpendingBurst))    == "burst");
}