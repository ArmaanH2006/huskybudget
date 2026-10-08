#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "budget/Date.h"
#include "budget/Money.h"
#include "budget/Transaction.h"

namespace budget {

// A calendar month, like October 2026.
struct YearMonth {
    int year  = 1970;
    int month = 1;

    static std::optional<YearMonth> parse(std::string_view text);   // "2026-10"
    std::string toString() const;                                    // "2026-10"
    Date firstDay() const;                                           // 2026-10-01
    Date lastDay() const;                                            // 2026-10-31
};

struct CategoryTotal {
    std::string category;
    Money       total;          // positive: how much was spent
};

struct MonthSummary {
    Money income;               // sum of money in (positive)
    Money spending;             // sum of money out, as a positive number
    Money net;                  // income - spending
    int   transactionCount = 0;
    std::vector<CategoryTotal> spendingByCategory;   // biggest first
};

// Pure calculation: no storage, no web. Transfers are ignored, since moving
// money between your own accounts is neither income nor spending.
MonthSummary summarize(const std::vector<Transaction>& transactions);

}  // namespace budget