#pragma once
#include <string>
#include <vector>

#include "budget/Budget.h"
#include "budget/Date.h"
#include "budget/Money.h"
#include "budget/Summary.h"
#include "budget/Transaction.h"

namespace budget {

// How a budget is doing this month.
//   OnTrack: fine so far
//   AtRisk:  not over yet, but at this pace it will be by the end of the month
//   Over:    already spent more than the limit
enum class BudgetHealth { OnTrack, AtRisk, Over };

// "on-track" / "at-risk" / "over"
const char* budgetHealthName(BudgetHealth health);

// Before this many days into the month, the pace isn't trusted.
// (One big grocery trip on the 2nd isn't a trend.)
constexpr int kMinDaysForPace = 7;

struct BudgetStatus {
    std::string  category;
    Money        limit;              // the monthly limit, e.g. $60.00
    Money        spent;              // spent so far this month, as a positive number
    Money        remaining;          // limit - spent (negative when over)
    int          percentUsed = 0;    // spent / limit * 100, rounded down
    Money        projected;          // best guess for the whole month at the current pace
    bool         tooEarly = false;   // true when it's too early in the month to judge the pace
    BudgetHealth health = BudgetHealth::OnTrack;
};

// Pure calculation: no storage, no web.
//
// `transactions` can be anything; only expenses inside `month` count.
// `today` decides how far into the month we are:
//   - a past month counts as finished (projected = spent)
//   - a future month hasn't started (tooEarly = true)
//
// The result has one entry per budget, the most used first.
std::vector<BudgetStatus> evaluateBudgets(const std::vector<Budget>& budgets,
                                          const std::vector<Transaction>& transactions,
                                          YearMonth month,
                                          Date today);

}  // namespace budget