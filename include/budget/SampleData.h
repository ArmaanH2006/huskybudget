#pragma once
#include <vector>

#include "budget/Date.h"
#include "budget/Transaction.h"

namespace budget {

// Makes a realistic-looking history of transactions for demos and testing:
// paychecks, rent, bills, groceries, dining, coffee, and so on, with a few
// deliberate oddities (a big TV purchase and a double charge in the last full
// month) so the alerts have something to find.
//
// Covers `months` whole months ending with the month of `lastDay`, and never
// creates anything after `lastDay`. The same `seed` always gives the same data.
std::vector<Transaction> generateSampleData(Date lastDay, int months, unsigned seed, int userId);

}  // namespace budget