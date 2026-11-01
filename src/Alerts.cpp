#include "budget/Alerts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace budget {

const char* alertKindName(AlertKind kind) {
    switch (kind) {
        case AlertKind::DuplicateCharge:  return "duplicate";
        case AlertKind::NewMerchantLarge: return "new-merchant";
        case AlertKind::SpendingBurst:    return "burst";
        case AlertKind::UnusualAmount:    break;
    }
    return "unusual-amount";
}

namespace {

bool isExpense(const Transaction& t) { return t.kind == TxnKind::Expense; }

// How much was spent, as a positive number of cents.
std::int64_t spentCents(const Transaction& t) { return -t.amount.cents(); }

// Counts days from Jan 1, 1900, so we can subtract two dates.
// (Date only knows year/month/day; this turns it into one number.)
int dayNumber(Date d) {
    int days = 0;
    for (int y = 1900; y < d.year; ++y)   days += Date::isLeapYear(y) ? 366 : 365;
    for (int m = 1; m < d.month; ++m)     days += Date::daysInMonth(d.year, m);
    return days + d.day;
}

// 4.21 -> "4.2"
std::string oneDecimal(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f", value);
    return buffer;
}

bool inMonth(const Transaction& t, YearMonth month) {
    return !(t.date < month.firstDay()) && !(month.lastDay() < t.date);
}

Alert makeAlert(AlertKind kind, const Transaction& t, std::string reason) {
    Alert a;
    a.kind          = kind;
    a.date          = t.date;
    a.transactionId = t.id;
    a.merchant      = t.merchant;
    a.category      = t.category;
    a.amount        = Money::fromCents(spentCents(t));
    a.reason        = std::move(reason);
    return a;
}

// ---- Rule 1: same store, same amount, same day as an earlier charge.
bool isDuplicate(const std::vector<Transaction>& history, std::size_t i) {
    const Transaction& t = history[i];
    if (t.merchant.empty()) return false;
    for (std::size_t j = 0; j < i; ++j) {
        const Transaction& other = history[j];
        if (isExpense(other) && other.date == t.date && other.merchant == t.merchant &&
            other.amount == t.amount) {
            return true;
        }
    }
    return false;
}

// ---- Rule 2: far above your normal charge in this category.
//
// "Normal" = the average of your earlier charges in the same category.
// "Far above" = more than 2.5 standard deviations above that average
//               (standard deviation = how much your charges usually vary)
//               AND at least 1.5 times the average.
bool isUnusualAmount(const std::vector<Transaction>& history, std::size_t i, std::string& reason) {
    const Transaction& t = history[i];

    std::vector<double> earlier;
    for (const Transaction& other : history) {
        if (isExpense(other) && other.category == t.category && other.date < t.date) {
            earlier.push_back(static_cast<double>(spentCents(other)));
        }
    }
    if (static_cast<int>(earlier.size()) < kMinHistoryForNormal) return false;   // don't know "normal" yet

    double sum = 0;
    for (double c : earlier) sum += c;
    const double average = sum / earlier.size();

    double squares = 0;
    for (double c : earlier) squares += (c - average) * (c - average);
    const double spread = std::sqrt(squares / earlier.size());     // standard deviation

    const double amount = static_cast<double>(spentCents(t));
    if (amount < average * kUnusualMinRatio) return false;
    // If every earlier charge was identical (spread 0), being 1.5x bigger is enough.
    if (spread > 0 && (amount - average) / spread <= kUnusualZScore) return false;

    reason = Money::fromCents(spentCents(t)).toString() + " is about " + oneDecimal(amount / average) +
             " times your usual " + t.category + " charge (average " +
             Money::fromCents(std::llround(average)).toString() + ")";
    return true;
}

// ---- Rule 3: a big charge at a store you've never used before.
bool isNewMerchantLarge(const std::vector<Transaction>& history, std::size_t i, int firstDay,
                        std::string& reason) {
    const Transaction& t = history[i];
    if (t.merchant.empty()) return false;
    if (spentCents(t) < kNewMerchantMinCents) return false;
    if (dayNumber(t.date) - firstDay < kMinDaysOfHistory) return false;   // everything is "new" at first

    for (std::size_t j = 0; j < i; ++j) {
        if (history[j].merchant == t.merchant) return false;              // been there before
    }
    reason = "First time at " + t.merchant + ", and it's a big charge (" +
             Money::fromCents(spentCents(t)).toString() + ")";
    return true;
}

}  // namespace

std::vector<Alert> findAlerts(const std::vector<Transaction>& history, YearMonth month) {
    std::vector<Alert> alerts;
    if (history.empty()) return alerts;

    int firstDay = dayNumber(history[0].date);                 // when the user's history starts
    for (const Transaction& t : history) firstDay = std::min(firstDay, dayNumber(t.date));

    // ---- Rules 1-3: look at each charge in the month. At most one alert per
    // charge; the rules are checked in order of how sure we are.
    for (std::size_t i = 0; i < history.size(); ++i) {
        const Transaction& t = history[i];
        if (!isExpense(t) || !inMonth(t, month)) continue;

        std::string reason;
        if (isDuplicate(history, i)) {
            alerts.push_back(makeAlert(AlertKind::DuplicateCharge, t,
                "Charged " + Money::fromCents(spentCents(t)).toString() + " at " + t.merchant +
                " twice on the same day. Check if you were billed twice."));
        } else if (isUnusualAmount(history, i, reason)) {
            alerts.push_back(makeAlert(AlertKind::UnusualAmount, t, reason));
        } else if (isNewMerchantLarge(history, i, firstDay, reason)) {
            alerts.push_back(makeAlert(AlertKind::NewMerchantLarge, t, reason));
        }
    }

    // ---- Rule 4: a day with far more purchases than you normally make.
    std::map<int, std::vector<const Transaction*>> byDay;      // day number -> that day's charges
    for (const Transaction& t : history) {
        if (isExpense(t) && inMonth(t, month)) byDay[dayNumber(t.date)].push_back(&t);
    }
    for (const auto& [day, charges] : byDay) {
        const int count = static_cast<int>(charges.size());
        if (count < kBurstMinPurchases) continue;

        const int daysBefore = day - firstDay;
        if (daysBefore < kMinDaysOfHistory) continue;          // not enough history for "normal"

        int earlierPurchases = 0;
        for (const Transaction& t : history) {
            if (isExpense(t) && dayNumber(t.date) < day) ++earlierPurchases;
        }
        const double perDay = static_cast<double>(earlierPurchases) / daysBefore;
        if (count < perDay * kBurstMinRatio) continue;

        std::int64_t total = 0;
        for (const Transaction* t : charges) total += spentCents(*t);

        Alert a;
        a.kind   = AlertKind::SpendingBurst;
        a.date   = charges.front()->date;
        a.amount = Money::fromCents(total);
        a.reason = std::to_string(count) + " purchases in one day, adding up to " + a.amount.toString() +
                   ". You usually make about " + oneDecimal(perDay) + " a day.";
        alerts.push_back(a);
    }

    // Newest first. stable_sort keeps same-day alerts in the order found.
    std::stable_sort(alerts.begin(), alerts.end(),
                     [](const Alert& a, const Alert& b) { return b.date < a.date; });
    return alerts;
}

}  // namespace budget