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