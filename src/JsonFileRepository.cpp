#include "budget/JsonFileRepository.h"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "json.hpp"

using nlohmann::json;

namespace budget {

namespace {

// ---- TxnKind <-> text ------------------------------------------------------
// Stored as words, not numbers, so the file is readable and reordering the
// enum later can't silently change what old files mean.

std::string kindToString(TxnKind kind) {
    switch (kind) {
        case TxnKind::Income:   return "income";
        case TxnKind::Transfer: return "transfer";
        case TxnKind::Expense:  break;
    }
    return "expense";
}

TxnKind kindFromString(const std::string& text) {
    if (text == "income")   return TxnKind::Income;
    if (text == "transfer") return TxnKind::Transfer;
    if (text == "expense")  return TxnKind::Expense;
    throw std::runtime_error("unknown transaction kind: " + text);
}

// ---- Transaction <-> JSON --------------------------------------------------
// Money is saved as an integer number of cents, never as a decimal, so
// nothing is lost going to the file and back.

json toJson(const Transaction& t) {
    return {
        {"id",       t.id},
        {"userId",   t.userId},
        {"date",     t.date.toIso()},
        {"cents",    t.amount.cents()},
        {"category", t.category},
        {"merchant", t.merchant},
        {"note",     t.note},
        {"kind",     kindToString(t.kind)},
    };
}

Transaction transactionFromJson(const json& j) {
    Transaction t;
    t.id     = j.at("id").get<int>();
    t.userId = j.at("userId").get<int>();

    auto date = Date::parse(j.at("date").get<std::string>());
    if (!date) throw std::runtime_error("bad date in file: " + j.at("date").dump());
    t.date = *date;

    t.amount   = Money::fromCents(j.at("cents").get<std::int64_t>());
    t.category = j.at("category").get<std::string>();
    t.merchant = j.value("merchant", "");      // optional fields get a default
    t.note     = j.value("note", "");
    t.kind     = kindFromString(j.value("kind", "expense"));
    return t;
}

// ---- Budget <-> JSON -------------------------------------------------------

json toJson(const Budget& b) {
    return {
        {"userId",     b.userId},
        {"category",   b.category},
        {"limitCents", b.monthlyLimit.cents()},
    };
}

Budget budgetFromJson(const json& j) {
    Budget b;
    b.userId       = j.at("userId").get<int>();
    b.category     = j.at("category").get<std::string>();
    b.monthlyLimit = Money::fromCents(j.at("limitCents").get<std::int64_t>());
    return b;
}

}  // namespace

// ---------------------------------------------------------------------------

JsonFileRepository::JsonFileRepository(std::string path) : path_(std::move(path)) {
    load();
}

int JsonFileRepository::addTransaction(const Transaction& txn) {
    int id = InMemoryRepository::addTransaction(txn);   // do the real work...
    save();                                             // ...then write it down
    return id;
}

bool JsonFileRepository::deleteTransaction(int id, int userId) {
    bool removed = InMemoryRepository::deleteTransaction(id, userId);
    if (removed) save();                                // nothing changed otherwise
    return removed;
}

void JsonFileRepository::setBudget(const Budget& budget) {
    InMemoryRepository::setBudget(budget);
    save();
}

void JsonFileRepository::load() {
    std::ifstream in(path_);
    if (!in) return;                                    // no file yet: start empty

    json root;
    try {
        in >> root;
    } catch (const json::exception& e) {
        throw std::runtime_error("could not read " + path_ + ": " + e.what());
    }

    try {
        nextId_ = root.value("nextId", 1);
        for (const json& j : root.at("transactions")) {
            transactions_.push_back(transactionFromJson(j));
        }
        for (const json& j : root.at("budgets")) {
            budgets_.push_back(budgetFromJson(j));
        }
    } catch (const json::exception& e) {
        throw std::runtime_error("bad data in " + path_ + ": " + e.what());
    }
}

void JsonFileRepository::save() const {
    json root;
    root["nextId"]       = nextId_;
    root["transactions"] = json::array();
    root["budgets"]      = json::array();
    for (const Transaction& t : transactions_) root["transactions"].push_back(toJson(t));
    for (const Budget& b : budgets_)          root["budgets"].push_back(toJson(b));

    std::ofstream out(path_);
    if (!out) throw std::runtime_error("could not write " + path_);
    out << root.dump(2);                                // 2 = indent, so it's readable
}

}  // namespace budget