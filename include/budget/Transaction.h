#pragma once
#include <optional>
#include <string>
#include <string_view>

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

// "expense" / "income" / "transfer"
inline const char* txnKindName(TxnKind kind) {
    switch (kind) {
        case TxnKind::Income:   return "income";
        case TxnKind::Transfer: return "transfer";
        case TxnKind::Expense:  break;
    }
    return "expense";
}

// The reverse. Returns nothing for anything else.
inline std::optional<TxnKind> parseTxnKind(std::string_view text) {
    if (text == "expense")  return TxnKind::Expense;
    if (text == "income")   return TxnKind::Income;
    if (text == "transfer") return TxnKind::Transfer;
    return std::nullopt;
}

}  // namespace budget