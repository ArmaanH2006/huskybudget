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