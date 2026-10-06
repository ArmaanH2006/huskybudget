#include "budget/InMemoryRepository.h"

#include <algorithm>

namespace budget {

int InMemoryRepository::addTransaction(const Transaction& txn) {
    Transaction stored = txn;      // copy, so we can set the id
    stored.id = nextId_;
    ++nextId_;
    transactions_.push_back(stored);
    return stored.id;
}

bool InMemoryRepository::deleteTransaction(int id, int userId) {
    for (auto it = transactions_.begin(); it != transactions_.end(); ++it) {
        if (it->id == id && it->userId == userId) {
            transactions_.erase(it);
            return true;
        }
    }
    return false;    // not found, or it belongs to someone else
}

std::vector<Transaction> InMemoryRepository::transactionsInRange(int userId, Date from, Date to) const {
    std::vector<Transaction> result;
    for (const Transaction& t : transactions_) {
        bool onOrAfterFrom = !(t.date < from);   // t.date >= from
        bool onOrBeforeTo  = !(to < t.date);     // t.date <= to
        if (t.userId == userId && onOrAfterFrom && onOrBeforeTo) {
            result.push_back(t);
        }
    }

    // Oldest first. stable_sort keeps same-day transactions in the order added.
    std::stable_sort(result.begin(), result.end(),
                     [](const Transaction& a, const Transaction& b) { return a.date < b.date; });
    return result;
}

void InMemoryRepository::setBudget(const Budget& budget) {
    for (Budget& existing : budgets_) {
        if (existing.userId == budget.userId && existing.category == budget.category) {
            existing.monthlyLimit = budget.monthlyLimit;   // update in place
            return;
        }
    }
    budgets_.push_back(budget);                            // new category
}

std::vector<Budget> InMemoryRepository::budgetsFor(int userId) const {
    std::vector<Budget> result;
    for (const Budget& b : budgets_) {
        if (b.userId == userId) {
            result.push_back(b);
        }
    }
    return result;
}

}  // namespace budget