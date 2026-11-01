#include "budget/BudgetStatus.h"

#include <algorithm>
#include <cstdint>
#include <map>

namespace budget {

const char* budgetHealthName(BudgetHealth health) {
    switch (health) {
        case BudgetHealth::AtRisk: return "at-risk";
        case BudgetHealth::Over:   return "over";
        case BudgetHealth::OnTrack: break;
    }
    return "on-track";
}

namespace {

// How many days of `month` have happened by `today`.
//   today before the month  -> 0
//   today inside the month  -> today's day number (Oct 8 -> 8)
//   today after the month   -> every day of the month
int daysElapsed(YearMonth month, Date today) {
    if (today < month.firstDay()) return 0;
    if (month.lastDay() < today) return Date::daysInMonth(month.year, month.month);
    return today.day;
}

// Total spent per category, as positive cents. Only expenses inside the month.
std::map<std::string, std::int64_t> spendingByCategory(const std::vector<Transaction>& transactions,
                                                       YearMonth month) {
    const Date first = month.firstDay();
    const Date last  = month.lastDay();

    std::map<std::string, std::int64_t> spent;
    for (const Transaction& t : transactions) {
        if (t.kind != TxnKind::Expense) continue;          // income and transfers aren't spending
        if (t.date < first || last < t.date) continue;     // outside this month
        spent[t.category] += -t.amount.cents();            // expenses are stored negative
    }
    return spent;
}

}  // namespace

std::vector<BudgetStatus> evaluateBudgets(const std::vector<Budget>& budgets,
                                          const std::vector<Transaction>& transactions,
                                          YearMonth month,
                                          Date today) {
    const auto spent       = spendingByCategory(transactions, month);
    const int  daysInMonth = Date::daysInMonth(month.year, month.month);
    const int  elapsed     = daysElapsed(month, today);

    std::vector<BudgetStatus> result;
    for (const Budget& b : budgets) {
        BudgetStatus s;
        s.category = b.category;
        s.limit    = b.monthlyLimit;

        auto found = spent.find(b.category);
        const std::int64_t spentCents = (found == spent.end()) ? 0 : found->second;
        const std::int64_t limitCents = b.monthlyLimit.cents();

        s.spent     = Money::fromCents(spentCents);
        s.remaining = s.limit - s.spent;

        if (limitCents > 0) {
            s.percentUsed = static_cast<int>(spentCents * 100 / limitCents);
        } else {
            s.percentUsed = (spentCents > 0) ? 100 : 0;
        }

        // The pace: spent so far / days so far * days in the month.
        // Done in whole cents, rounded to the nearest cent.
        if (elapsed >= daysInMonth) {
            s.projected = s.spent;                          // month is over: nothing left to guess
        } else if (elapsed < kMinDaysForPace) {
            s.tooEarly  = true;
            s.projected = s.spent;                          // not enough days to trust a pace
        } else {
            s.projected = Money::fromCents((spentCents * daysInMonth + elapsed / 2) / elapsed);
        }

        if (s.limit < s.spent) {
            s.health = BudgetHealth::Over;
        } else if (!s.tooEarly && s.limit < s.projected) {
            s.health = BudgetHealth::AtRisk;
        } else {
            s.health = BudgetHealth::OnTrack;
        }

        result.push_back(s);
    }

    // Most used first, so the budgets that need attention are at the top.
    // Ties go alphabetically so the order never jumps around.
    std::sort(result.begin(), result.end(), [](const BudgetStatus& a, const BudgetStatus& b) {
        if (a.percentUsed != b.percentUsed) return a.percentUsed > b.percentUsed;
        return a.category < b.category;
    });
    return result;
}

}  // namespace budget