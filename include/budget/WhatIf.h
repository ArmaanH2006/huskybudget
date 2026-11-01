#pragma once
#include <vector>

#include "budget/Money.h"

namespace budget {

// "What if I saved $200 a month at 7% a year for 10 years?"
struct WhatIfResult {
    Money monthly;                     // put in every month
    double annualRatePercent = 0;      // e.g. 7.0
    int years = 0;

    Money contributed;                 // everything you put in: monthly * 12 * years
    Money finalBalance;                // what it grows to
    Money growth;                      // finalBalance - contributed: what the interest earned
    std::vector<Money> balanceByYear;  // balance at the end of year 1, 2, ..., years
};

// Puts `monthly` in at the end of every month, with interest added monthly
// (the yearly rate / 12). Uses the standard savings formula:
//
//     balance = monthly * ((1 + r)^n - 1) / r      r = monthly rate, n = months
//
// $200 a month at 7% for 10 years -> $34,616.96.
// A rate of 0 just adds up the deposits.
WhatIfResult whatIf(Money monthly, double annualRatePercent, int years);

}  // namespace budget