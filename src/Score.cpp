#include "budget/Score.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>

#include "budget/Alerts.h"
#include "budget/BudgetStatus.h"

namespace budget {

namespace {

// The "wants" categories. Everything else (rent, groceries, bills...) is a need.
const std::set<std::string> kWants = {"Dining", "Coffee", "Shopping", "Entertainment"};

constexpr double kSavingsGoal   = 0.20;   // keep 20% of income
constexpr double kWantsOk       = 0.30;   // up to 30% on wants is fine...
constexpr double kWantsTooMuch  = 0.50;   // ...50% or more gets no points
constexpr double kTrendTooMuch  = 0.25;   // 25% above your usual gets no points

// Turns "how good is this, from 0.0 to 1.0" into whole points.
int pointsFor(double fraction, int maxPoints) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    return static_cast<int>(std::lround(fraction * maxPoints));
}

int percent(double fraction) { return static_cast<int>(std::lround(fraction * 100)); }

YearMonth previousMonth(YearMonth m) {
    if (--m.month == 0) { m.month = 12; --m.year; }
    return m;
}

std::vector<Transaction> inMonth(const std::vector<Transaction>& history, YearMonth month) {
    std::vector<Transaction> result;
    for (const Transaction& t : history) {
        if (!(t.date < month.firstDay()) && !(month.lastDay() < t.date)) result.push_back(t);
    }
    return result;
}

// Days of `month` that have happened by `today` (same idea as in BudgetStatus.cpp).
int daysElapsed(YearMonth month, Date today) {
    if (today < month.firstDay()) return 0;
    if (month.lastDay() < today) return Date::daysInMonth(month.year, month.month);
    return today.day;
}

}  // namespace

std::string gradeFor(int total) {
    if (total >= 90) return "A";
    if (total >= 80) return "B";
    if (total >= 70) return "C";
    if (total >= 60) return "D";
    return "F";
}

SpendingScore scoreMonth(const std::vector<Transaction>& history,
                         const std::vector<Budget>& budgets,
                         YearMonth month,
                         Date today) {
    SpendingScore score;
    const std::vector<Transaction> thisMonth = inMonth(history, month);
    const MonthSummary summary = summarize(thisMonth);
    const std::int64_t income   = summary.income.cents();
    const std::int64_t spending = summary.spending.cents();

    // ---- 1. Savings (30): did you keep 20% of what you earned?
    double savingsRate = 0;
    {
        ScorePart p{"Savings", 0, 30, ""};
        if (income <= 0) {
            p.detail = "No income recorded this month";
        } else {
            savingsRate = static_cast<double>(income - spending) / income;
            p.points = pointsFor(savingsRate / kSavingsGoal, p.maxPoints);
            p.detail = (savingsRate >= 0)
                ? "You kept " + std::to_string(percent(savingsRate)) + "% of your income (goal: 20%)"
                : "You spent " + std::to_string(-percent(savingsRate)) + "% more than you earned (goal: keep 20%)";
        }
        score.parts.push_back(p);
    }

    // ---- 2. Budgets (25): how many stayed under their limit?
    const std::vector<BudgetStatus> statuses = evaluateBudgets(budgets, thisMonth, month, today);
    {
        ScorePart p{"Budgets", 0, 25, ""};
        if (statuses.empty()) {
            p.detail = "No budgets set yet";
        } else {
            int kept = 0;
            for (const BudgetStatus& b : statuses) {
                if (b.health != BudgetHealth::Over) ++kept;
            }
            p.points = pointsFor(static_cast<double>(kept) / statuses.size(), p.maxPoints);
            p.detail = std::to_string(kept) + " of " + std::to_string(statuses.size()) +
                       " budgets stayed under the limit";
        }
        score.parts.push_back(p);
    }

    // ---- 3. Steady spending (20): this month vs. the average of the last 3.
    {
        ScorePart p{"Steady spending", 20, 20, ""};

        std::int64_t pastTotal = 0;
        int pastMonths = 0;
        YearMonth m = month;
        for (int i = 0; i < 3; ++i) {
            m = previousMonth(m);
            std::vector<Transaction> txns = inMonth(history, m);
            if (txns.empty()) continue;                   // no data for that month
            pastTotal += summarize(txns).spending.cents();
            ++pastMonths;
        }

        const int elapsed     = daysElapsed(month, today);
        const int daysInMonth = Date::daysInMonth(month.year, month.month);

        if (pastMonths == 0) {
            p.detail = "Not enough history to compare yet";
        } else if (elapsed < kMinDaysForPace) {
            p.detail = "Too early in the month to compare";
        } else if (pastTotal <= 0) {
            p.detail = "No spending in recent months to compare with";
        } else {
            // For a month that's only partly over, compare against the same
            // share of a normal month (10 days of 31 -> 10/31 of the average).
            const double usual = static_cast<double>(pastTotal) / pastMonths * elapsed / daysInMonth;
            const double ratio = spending / usual;              // 1.10 = 10% more than usual
            p.points = pointsFor(1.0 - (ratio - 1.0) / kTrendTooMuch, p.maxPoints);
            const int diff = percent(ratio - 1.0);
            p.detail = (diff >= 0)
                ? "Spending is " + std::to_string(diff) + "% above your recent average"
                : "Spending is " + std::to_string(-diff) + "% below your recent average";
        }
        score.parts.push_back(p);
    }

    // ---- 4. Needs vs wants (15): how much went to "wants"?
    std::string biggestWant;
    std::int64_t biggestWantCents = 0;
    {
        ScorePart p{"Needs vs wants", 15, 15, ""};
        std::int64_t wants = 0;
        for (const CategoryTotal& c : summary.spendingByCategory) {
            if (kWants.count(c.category) == 0) continue;
            wants += c.total.cents();
            if (c.total.cents() > biggestWantCents) {
                biggestWant = c.category;
                biggestWantCents = c.total.cents();
            }
        }
        if (spending > 0) {
            const double share = static_cast<double>(wants) / spending;
            p.points = pointsFor((kWantsTooMuch - share) / (kWantsTooMuch - kWantsOk), p.maxPoints);
            p.detail = std::to_string(percent(share)) + "% of spending went to wants (goal: 30% or less)";
        } else {
            p.detail = "No spending yet this month";
        }
        score.parts.push_back(p);
    }

    // ---- 5. No surprises (10): lose 5 points per alert.
    const std::vector<Alert> alerts = findAlerts(history, month);
    {
        ScorePart p{"No surprises", 0, 10, ""};
        p.points = std::max(0, 10 - 5 * static_cast<int>(alerts.size()));
        p.detail = alerts.empty() ? "No alerts this month"
                                  : std::to_string(alerts.size()) + " alert(s) this month";
        score.parts.push_back(p);
    }

    for (const ScorePart& p : score.parts) score.total += p.points;
    score.grade = gradeFor(score.total);

    // ---- Advice: one tip per thing that cost points, most important first.
    if (income > 0 && savingsRate < kSavingsGoal) {
        const std::int64_t needed = std::llround(income * kSavingsGoal) - (income - spending);
        const std::string start = (savingsRate >= 0)
            ? "You kept " + std::to_string(percent(savingsRate)) + "% of your income. "
            : "You spent more than you earned. ";
        score.tips.push_back(start + "Saving " + Money::fromCents(needed).toString() +
                             " more would reach the 20% goal.");
    }
    for (const BudgetStatus& b : statuses) {
        if (b.health == BudgetHealth::Over) {
            score.tips.push_back("You went over your " + b.category + " budget by " +
                                 (-b.remaining).toString() + ".");
        }
    }
    for (const BudgetStatus& b : statuses) {
        if (b.health == BudgetHealth::AtRisk) {
            score.tips.push_back(b.category + " is on pace for about " + b.projected.toString() +
                                 " against a " + b.limit.toString() +
                                 " limit. Slowing down now keeps you under.");
        }
    }
    if (score.parts[3].points < score.parts[3].maxPoints && !biggestWant.empty()) {
        score.tips.push_back(biggestWant + " was your biggest want at " +
                             Money::fromCents(biggestWantCents).toString() +
                             ". Cutting it by a quarter would free up about " +
                             Money::fromCents(biggestWantCents / 4).toString() + " a month.");
    }
    if (!alerts.empty()) {
        score.tips.push_back("Check this month's alerts. A double charge can usually be refunded "
                             "by the store or your bank.");
    }
    if (budgets.empty()) {
        score.tips.push_back("Set budgets for your biggest categories so the app can warn you early.");
    }
    if (score.tips.empty()) {
        score.tips.push_back("Nice month. Everything looks healthy.");
    }
    return score;
}

}  // namespace budget