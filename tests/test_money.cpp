#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "budget/Money.h"

TEST_CASE("plumbing works") {
    CHECK(1 + 1 == 2);
}

TEST_CASE("fromCents round-trips") {
    CHECK(budget::Money::fromCents(500).cents() == 500);
    CHECK(budget::Money::fromCents(-250).cents() == -250);
    CHECK(budget::Money().cents() == 0);
}

TEST_CASE("arithmetic") {
    using budget::Money;
    CHECK((Money::fromCents(100) + Money::fromCents(250)).cents() == 350);
    CHECK((Money::fromCents(100) - Money::fromCents(250)).cents() == -150);
    CHECK((-Money::fromCents(500)).cents() == -500);
}

TEST_CASE("comparison") {
    using budget::Money;
    CHECK(Money::fromCents(100) == Money::fromCents(100));
    CHECK(Money::fromCents(100) != Money::fromCents(101));
    CHECK(Money::fromCents(-5) < Money::fromCents(3));
    CHECK_FALSE(Money::fromCents(3) < Money::fromCents(-5));
}

TEST_CASE("ten dimes make exactly one dollar") {
    using budget::Money;
    Money total;
    for (int i = 0; i < 10; ++i) {
        total = total + Money::fromCents(10);
    }
    CHECK(total == Money::fromCents(100));
}

TEST_CASE("toString") {
    using budget::Money;
    CHECK(Money::fromCents(500).toString()       == "$5.00");
    CHECK(Money::fromCents(105).toString()       == "$1.05");     // the zero problem
    CHECK(Money::fromCents(0).toString()         == "$0.00");
    CHECK(Money::fromCents(-5).toString()        == "-$0.05");    // the negative problem
    CHECK(Money::fromCents(-150).toString()      == "-$1.50");
    CHECK(Money::fromCents(123456).toString()    == "$1,234.56");
    CHECK(Money::fromCents(99999).toString()     == "$999.99");   // no comma yet
    CHECK(Money::fromCents(100000000).toString() == "$1,000,000.00");
}

TEST_CASE("parse accepts valid input") {
    using budget::Money;
    CHECK(Money::parse("5")->cents()          == 500);
    CHECK(Money::parse("5.1")->cents()        == 510);
    CHECK(Money::parse("5.12")->cents()       == 512);
    CHECK(Money::parse("1.005")->cents()      == 101);   // double would give 100
    CHECK(Money::parse("9.999")->cents()      == 1000);  // rounding carries into dollars
    CHECK(Money::parse("$1,234.56")->cents()  == 123456);
    CHECK(Money::parse("-$5.00")->cents()     == -500);
    CHECK(Money::parse("$-5.00")->cents()     == -500);
    CHECK(Money::parse(".50")->cents()        == 50);
    CHECK(Money::parse("  5.00  ")->cents()   == 500);
    CHECK(Money::parse("-0.01")->cents()      == -1);
}

TEST_CASE("parse rejects garbage") {
    using budget::Money;
    for (const char* bad : {"", "$", ".", "-", "5.", "1.2.3", "abc", "12a",
                            "--5", "$$5", ",5"}) {
        INFO("input: \"", bad, "\"");
        CHECK_FALSE(Money::parse(bad).has_value());
    }
}

TEST_CASE("parse and toString round-trip") {
    using budget::Money;
    for (const char* s : {"$0.00", "$5.00", "-$0.05", "$1,234.56", "-$1,000,000.00"}) {
        INFO("input: ", s);
        CHECK(Money::parse(s)->toString() == s);
    }
}