#pragma once
#include <string>
#include <vector>

#include "budget/Budget.h"
#include "budget/Date.h"
#include "budget/Summary.h"
#include "budget/Transaction.h"

namespace budget {

// One line of the score breakdown, e.g. "Savings: 18 of 30 points".
struct ScorePart {
    std::string name;          // "Savings"
    int         points    = 0;
    int         maxPoints = 0;
    std::string detail;        // "You kept 12% of your income (goal: 20%)"
};

struct SpendingScore {
    int                      total = 0;   // 0 to 100
    std::string              grade;       // "A", "B", "C", "D", or "F"
    std::vector<ScorePart>   parts;       // the five parts, in order
    std::vector<std::string> tips;        // plain-English advice, most important first
};

// The five parts and their weights (they add up to 100):
//   Savings          30  how much of your income you kept (goal: 20%)
//   Budgets          25  how many budgets stayed under their limit
//   Steady spending  20  this month compared to your last 3 months
//   Needs vs wants   15  how much went to Dining, Coffee, Shopping, Entertainment
//   No surprises     10  how many alerts the month has
//
// Pure calculation: no storage, no web. `history` should be every transaction
// up to the end of `month`, oldest first. `today` matters for the current
// month, which is only partly over.
SpendingScore scoreMonth(const std::vector<Transaction>& history,
                         const std::vector<Budget>& budgets,
                         YearMonth month,
                         Date today);

// 93 -> "A", 85 -> "B", 72 -> "C", 64 -> "D", 40 -> "F"
std::string gradeFor(int total);

}  // namespace budget