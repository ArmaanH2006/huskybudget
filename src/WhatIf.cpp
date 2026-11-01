#include "budget/WhatIf.h"

#include <cmath>
#include <cstdint>

namespace budget {

namespace {

// Balance after `months` monthly deposits, rounded to the nearest cent.
// This is the one place a decimal (double) is used for money: compound
// interest needs powers like 1.00583^120, which whole cents can't do. The
// answer is rounded back to whole cents right away.
Money balanceAfter(Money monthly, double monthlyRate, int months) {
    const double deposit = static_cast<double>(monthly.cents());
    double cents;
    if (monthlyRate == 0) {
        cents = deposit * months;                                       // no interest
    } else {
        cents = deposit * (std::pow(1 + monthlyRate, months) - 1) / monthlyRate;
    }
    return Money::fromCents(std::llround(cents));
}

}  // namespace

WhatIfResult whatIf(Money monthly, double annualRatePercent, int years) {
    WhatIfResult r;
    r.monthly           = monthly;
    r.annualRatePercent = annualRatePercent;
    r.years             = years;

    const double monthlyRate = annualRatePercent / 100.0 / 12.0;        // 7% -> 0.005833...
    for (int y = 1; y <= years; ++y) {
        r.balanceByYear.push_back(balanceAfter(monthly, monthlyRate, y * 12));
    }

    r.contributed  = Money::fromCents(monthly.cents() * 12 * years);
    r.finalBalance = years > 0 ? r.balanceByYear.back() : Money();
    r.growth       = r.finalBalance - r.contributed;
    return r;
}

}  // namespace budget