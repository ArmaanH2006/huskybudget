#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "budget/Date.h"
#include "budget/Money.h"
#include "budget/Summary.h"
#include "budget/Transaction.h"

namespace budget {

// The four kinds of "this looks wrong" the app can spot.
enum class AlertKind {
    UnusualAmount,     // way bigger than your normal charge in that category
    DuplicateCharge,   // same store, same amount, same day, twice
    NewMerchantLarge,  // a big charge at a store you've never used before
    SpendingBurst,     // a lot more purchases in one day than you normally make
};

// "unusual-amount" / "duplicate" / "new-merchant" / "burst"
const char* alertKindName(AlertKind kind);

// ---- The rules' settings, in one place so they're easy to explain and tweak.

// Unusual amount: need this many earlier charges in the category to know "normal".
constexpr int    kMinHistoryForNormal = 5;
// ...and the charge must be this many standard deviations above the average...
constexpr double kUnusualZScore       = 2.5;
// ...and at least this many times the average (so tiny wiggles don't count).
constexpr double kUnusualMinRatio     = 1.5;

// New merchant: a first-time store only counts if the charge is at least this big...
constexpr std::int64_t kNewMerchantMinCents = 20000;   // $200.00
// ...and you have at least this many days of history (otherwise every store is "new").
constexpr int    kMinDaysOfHistory    = 30;

// Spending burst: at least this many purchases in one day...
constexpr int    kBurstMinPurchases   = 5;
// ...and at least this many times your normal number of purchases per day.
constexpr double kBurstMinRatio       = 3.0;

struct Alert {
    AlertKind   kind = AlertKind::UnusualAmount;
    Date        date;
    int         transactionId = 0;   // the charge that caused it (0 for a whole-day burst)
    std::string merchant;
    std::string category;
    Money       amount;              // positive; for a burst, the day's total
    std::string reason;              // plain English, shown to the user
};

// Pure calculation: no storage, no web.
//
// `history` should be every transaction up to the end of `month`, oldest first
// (the earlier months are how the rules learn what "normal" looks like).
// Only charges inside `month` get alerts. Each charge gets at most one alert.
// The result is newest first.
std::vector<Alert> findAlerts(const std::vector<Transaction>& history, YearMonth month);

}  // namespace budget