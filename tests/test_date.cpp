#include "doctest.h"
#include "budget/Date.h"

using budget::Date;

TEST_CASE("leap years") {
    CHECK(Date::isLeapYear(2024));
    CHECK_FALSE(Date::isLeapYear(2023));
    CHECK_FALSE(Date::isLeapYear(1900));   // century, not divisible by 400
    CHECK(Date::isLeapYear(2000));         // divisible by 400
}

TEST_CASE("days in month") {
    CHECK(Date::daysInMonth(2026, 1)  == 31);
    CHECK(Date::daysInMonth(2026, 4)  == 30);
    CHECK(Date::daysInMonth(2026, 2)  == 28);
    CHECK(Date::daysInMonth(2024, 2)  == 29);
    CHECK(Date::daysInMonth(2026, 12) == 31);
}

TEST_CASE("parse valid dates") {
    auto d = Date::parse("2026-10-05");
    REQUIRE(d.has_value());
    CHECK(d->year  == 2026);
    CHECK(d->month == 10);
    CHECK(d->day   == 5);

    CHECK(Date::parse("2024-02-29").has_value());   // leap day exists in 2024
}

TEST_CASE("parse rejects bad dates") {
    for (const char* bad : {"", "2026-10-5", "26-10-05", "2026/10/05",
                            "2026-13-01", "2026-00-10", "2026-02-30",
                            "2023-02-29", "2026-04-31", "abcd-ef-gh"}) {
        INFO("input: ", bad);
        CHECK_FALSE(Date::parse(bad).has_value());
    }
}

TEST_CASE("toIso pads with zeros") {
    CHECK(Date{2026, 3, 7}.toIso()  == "2026-03-07");
    CHECK(Date{2026, 12, 25}.toIso() == "2026-12-25");
}

TEST_CASE("parse and toIso round-trip") {
    for (const char* s : {"2026-01-01", "2026-10-05", "2024-02-29", "1999-12-31"}) {
        INFO("input: ", s);
        CHECK(Date::parse(s)->toIso() == s);
    }
}

TEST_CASE("date comparison") {
    Date a{2026, 10, 5};
    Date b{2026, 10, 6};
    Date c{2026, 11, 1};
    Date d{2027, 1, 1};

    CHECK(a == Date{2026, 10, 5});
    CHECK(a != b);
    CHECK(a < b);            // day decides
    CHECK(b < c);            // month decides, even though 6 > 1
    CHECK(c < d);            // year decides, even though 11 > 1
    CHECK_FALSE(b < a);
    CHECK_FALSE(a < a);      // a date is never less than itself
}