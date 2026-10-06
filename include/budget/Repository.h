#pragma once
#include <vector>

#include "budget/Budget.h"
#include "budget/Date.h"
#include "budget/Transaction.h"

namespace budget {

// The rules every storage system must follow. This class has no data and no
// code of its own; it only promises what functions exist.
//
// The rest of the app talks to a Repository& and never knows whether the
// data is in memory, in a JSON file, or (later) in SQLite.
class Repository {
public:
    virtual ~Repository() = default;

    // Stores a transaction, gives it a fresh id, and returns that id.
    virtual int addTransaction(const Transaction& txn) = 0;

    // Returns false if no transaction with that id belongs to that user.
    virtual bool deleteTransaction(int id, int userId) = 0;

    // Every transaction for the user with from <= date <= to, oldest first.
    virtual std::vector<Transaction> transactionsInRange(int userId, Date from, Date to) const = 0;

    // Adds the budget, or replaces the limit if that category already has one.
    virtual void setBudget(const Budget& budget) = 0;

    virtual std::vector<Budget> budgetsFor(int userId) const = 0;
};

}  // namespace budget