#pragma once
#include <vector>

#include "budget/Repository.h"

namespace budget {

// Keeps everything in vectors. Nothing is saved when the program exits.
// Used by the tests, and as the base for the JSON-file version next.
class InMemoryRepository : public Repository {
public:
    int  addTransaction(const Transaction& txn) override;
    bool deleteTransaction(int id, int userId) override;
    std::vector<Transaction> transactionsInRange(int userId, Date from, Date to) const override;

    void setBudget(const Budget& budget) override;
    std::vector<Budget> budgetsFor(int userId) const override;

protected:
    std::vector<Transaction> transactions_;
    std::vector<Budget>      budgets_;
    int                      nextId_ = 1;
};

}  // namespace budget