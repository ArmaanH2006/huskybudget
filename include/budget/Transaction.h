#pragma once
#include <string>

#include "budget/Date.h"
#include "budget/Money.h"

namespace budget {

enum class TxnKind { Expense, Income, Transfer };

struct Transaction {
    int         id     = 0;
    int         userId = 0;
    Date        date;                 // defaults to 1970-01-01
    Money       amount;               // negative = money out, positive = money in
    std::string category;             // "Dining", "Groceries", ...
    std::string merchant;             // "Chipotle"
    std::string note;
    TxnKind     kind = TxnKind::Expense;
};

}  // namespace budget