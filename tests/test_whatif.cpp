#include "doctest.h"
#include "budget/WhatIf.h"

using namespace budget;

TEST_CASE("$200 a month at 7% for 10 years") {
    WhatIfResult r = whatIf(Money::fromCents(20000), 7.0, 10);
    CHECK(r.finalBalance == Money::fromCents(3461696));     // $34,616.96
    CHECK(r.contributed  == Money::fromCents(2400000));     // $24,000.00 put in
    CHECK(r.growth       == Money::fromCents(1061696));     // $10,616.96 from interest
    REQUIRE(r.balanceByYear.size() == 10);
    CHECK(r.balanceByYear[0] == Money::fromCents(247852));  // $2,478.52 after year 1
    CHECK(r.balanceByYear.back() == r.finalBalance);
}

TEST_CASE("the balance grows every year") {
    WhatIfResult r = whatIf(Money::fromCents(10000), 5.0, 5);
    for (std::size_t i = 1; i < r.balanceByYear.size(); ++i) {
        CHECK(r.balanceByYear[i - 1] < r.balanceByYear[i]);
    }
}

TEST_CASE("$50 a month at 5% for 30 years") {
    WhatIfResult r = whatIf(Money::fromCents(5000), 5.0, 30);
    CHECK(r.finalBalance == Money::fromCents(4161293));     // $41,612.93
    CHECK(r.contributed  == Money::fromCents(1800000));     // $18,000.00
}

TEST_CASE("0% interest just adds up the deposits") {
    WhatIfResult r = whatIf(Money::fromCents(20000), 0.0, 10);
    CHECK(r.finalBalance == Money::fromCents(2400000));
    CHECK(r.growth == Money());
}

TEST_CASE("zero years means nothing saved yet") {
    WhatIfResult r = whatIf(Money::fromCents(20000), 7.0, 0);
    CHECK(r.finalBalance == Money());
    CHECK(r.balanceByYear.empty());
}