#include "budget/Summary.h"

#include <algorithm>
#include <map>

namespace budget {

std::optional<YearMonth> YearMonth::parse(std::string_view text) {
    // Reuse Date's careful validation: "2026-10" is valid if "2026-10-01" is.
    if (text.size() != 7) return std::nullopt;
    auto day = Date::parse(std::string(text) + "-01");
    if (!day) return std::nullopt;
    return YearMonth{day->year, day->month};
}

std::string YearMonth::toString() const {
    return firstDay().toIso().substr(0, 7);       // "2026-10-01" -> "2026-10"
}

Date YearMonth::firstDay() const {
    return Date{year, month, 1};
}

Date YearMonth::lastDay() const {
    return Date{year, month, Date::daysInMonth(year, month)};
}

MonthSummary summarize(const std::vector<Transaction>& transactions) {
    MonthSummary s;
    std::map<std::string, Money> byCategory;

    for (const Transaction& t : transactions) {
        if (t.kind == TxnKind::Transfer) continue;
        ++s.transactionCount;

        if (t.amount < Money()) {                 // money out
            Money spent = -t.amount;              // store as a positive number
            s.spending = s.spending + spent;
            byCategory[t.category] = byCategory[t.category] + spent;
        } else {                                  // money in
            s.income = s.income + t.amount;
        }
    }
    s.net = s.income - s.spending;

    for (const auto& [category, total] : byCategory) {
        s.spendingByCategory.push_back({category, total});
    }
    // Biggest spending first. Ties keep alphabetical order (std::map is sorted).
    std::stable_sort(s.spendingByCategory.begin(), s.spendingByCategory.end(),
                     [](const CategoryTotal& a, const CategoryTotal& b) { return b.total < a.total; });
    return s;
}

}  // namespace budget