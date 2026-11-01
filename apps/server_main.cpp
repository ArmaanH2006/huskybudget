// HuskyBudget web server.
//
// Run it from the project folder (the one containing web/ and data/):
//     .\build\huskybudget.exe
// then open http://localhost:8080 in a browser.

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

#include "httplib.h"
#include "json.hpp"

#include "budget/Alerts.h"
#include "budget/BudgetStatus.h"
#include "budget/CsvImporter.h"
#include "budget/JsonFileRepository.h"
#include "budget/Score.h"
#include "budget/Summary.h"
#include "budget/WhatIf.h"

using nlohmann::json;
using namespace budget;

namespace {

constexpr int kUserId = 1;            // single user until login exists
constexpr int kPort   = 8080;

// ---------------------------------------------------------------------------
// Turning our types into the JSON the browser receives.
// Every amount is sent twice: a display string ("-$12.50") and exact cents.
// ---------------------------------------------------------------------------

json moneyJson(Money m) {
    return {{"display", m.toString()}, {"cents", m.cents()}};
}

json toApiJson(const Transaction& t) {
    return {
        {"id",       t.id},
        {"date",     t.date.toIso()},
        {"amount",   t.amount.toString()},
        {"cents",    t.amount.cents()},
        {"category", t.category},
        {"merchant", t.merchant},
        {"note",     t.note},
        {"kind",     txnKindName(t.kind)},
    };
}

json toApiJson(const YearMonth& month, const MonthSummary& s) {
    json categories = json::array();
    for (const CategoryTotal& c : s.spendingByCategory) {
        categories.push_back({{"category", c.category}, {"amount", moneyJson(c.total)}});
    }
    return {
        {"month",              month.toString()},
        {"income",             moneyJson(s.income)},
        {"spending",           moneyJson(s.spending)},
        {"net",                moneyJson(s.net)},
        {"transactionCount",   s.transactionCount},
        {"spendingByCategory", categories},
    };
}

json toApiJson(const BudgetStatus& b) {
    return {
        {"category",    b.category},
        {"limit",       moneyJson(b.limit)},
        {"spent",       moneyJson(b.spent)},
        {"remaining",   moneyJson(b.remaining)},
        {"projected",   moneyJson(b.projected)},
        {"percentUsed", b.percentUsed},
        {"tooEarly",    b.tooEarly},
        {"health",      budgetHealthName(b.health)},
    };
}

json toApiJson(const Alert& a) {
    return {
        {"kind",          alertKindName(a.kind)},
        {"date",          a.date.toIso()},
        {"transactionId", a.transactionId},
        {"merchant",      a.merchant},
        {"category",      a.category},
        {"amount",        moneyJson(a.amount)},
        {"reason",        a.reason},
    };
}

json toApiJson(const SpendingScore& s) {
    json parts = json::array();
    for (const ScorePart& p : s.parts) {
        parts.push_back({{"name", p.name}, {"points", p.points}, {"maxPoints", p.maxPoints},
                         {"detail", p.detail}});
    }
    return {{"total", s.total}, {"grade", s.grade}, {"parts", parts}, {"tips", s.tips}};
}

json toApiJson(const WhatIfResult& r) {
    json byYear = json::array();
    for (Money m : r.balanceByYear) byYear.push_back(moneyJson(m));
    return {
        {"monthly",       moneyJson(r.monthly)},
        {"rate",          r.annualRatePercent},
        {"years",         r.years},
        {"contributed",   moneyJson(r.contributed)},
        {"finalBalance",  moneyJson(r.finalBalance)},
        {"growth",        moneyJson(r.growth)},
        {"balanceByYear", byYear},
    };
}

// Reads a plain number like "7" or "6.5". Returns false for "", "abc", or "7%".
bool parseNumber(const std::string& text, double& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    out = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size();      // the whole text must be the number
}

// Today's date on this computer.
Date today() {
    std::time_t now = std::time(nullptr);
    std::tm local = *std::localtime(&now);
    return Date{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

void sendError(httplib::Response& res, int status, const std::string& message) {
    res.status = status;
    res.set_content(json{{"error", message}}.dump(), "application/json");
}

// ---------------------------------------------------------------------------
// Reading a new transaction from the browser.
//
// Expects JSON like:
//   {"date":"2026-10-07", "amount":"12.50", "kind":"expense",
//    "category":"Dining", "merchant":"Chipotle", "note":""}
//
// The user always types a positive amount. "kind" decides the sign:
// expenses and transfers are stored negative, income positive.
// On bad input, fills in `error` and returns nothing.
// ---------------------------------------------------------------------------
std::optional<Transaction> transactionFromRequest(const std::string& body, std::string& error) {
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) { error = "body must be a JSON object"; return std::nullopt; }

    auto text = [&](const char* key) -> std::string {
        if (!j.contains(key) || !j[key].is_string()) return "";
        return j[key].get<std::string>();
    };

    Transaction t;
    t.userId = kUserId;

    auto date = Date::parse(text("date"));
    if (!date) { error = "date must look like 2026-10-07"; return std::nullopt; }
    t.date = *date;

    auto amount = Money::parse(text("amount"));
    if (!amount)              { error = "amount must be a number like 12.50"; return std::nullopt; }
    if (!(Money() < *amount)) { error = "amount must be more than $0.00";     return std::nullopt; }

    std::string kindText = text("kind").empty() ? "expense" : text("kind");
    auto kind = parseTxnKind(kindText);
    if (!kind) { error = "kind must be expense, income, or transfer"; return std::nullopt; }
    t.kind   = *kind;
    t.amount = (t.kind == TxnKind::Income) ? *amount : -*amount;

    t.category = text("category");
    if (t.category.empty()) { error = "category is required"; return std::nullopt; }
    t.merchant = text("merchant");
    t.note     = text("note");
    return t;
}

// Reads ?month=YYYY-MM, or ?from=&to=. Defaults to "everything".
bool readDateRange(const httplib::Request& req, Date& from, Date& to, std::string& error) {
    from = Date{1900, 1, 1};
    to   = Date{9999, 12, 31};
    if (req.has_param("month")) {
        auto m = YearMonth::parse(req.get_param_value("month"));
        if (!m) { error = "month must look like 2026-10"; return false; }
        from = m->firstDay();
        to   = m->lastDay();
        return true;
    }
    if (req.has_param("from")) {
        auto d = Date::parse(req.get_param_value("from"));
        if (!d) { error = "bad 'from' date"; return false; }
        from = *d;
    }
    if (req.has_param("to")) {
        auto d = Date::parse(req.get_param_value("to"));
        if (!d) { error = "bad 'to' date"; return false; }
        to = *d;
    }
    return true;
}

// A handful of transactions so the page isn't empty the first time.
// The real 12-month seed generator comes later this week.
void addSampleDataIfEmpty(Repository& repo) {
    if (!repo.transactionsInRange(kUserId, Date{1900, 1, 1}, Date{9999, 12, 31}).empty()) return;

    struct Sample { const char* date; const char* amount; const char* category;
                    const char* merchant; TxnKind kind; };
    const Sample samples[] = {
        {"2026-10-01", "2400.00",  "Salary",        "Employer",    TxnKind::Income},
        {"2026-10-01", "-1100.00", "Rent",          "Landlord",    TxnKind::Expense},
        {"2026-10-02", "-64.18",   "Groceries",     "Stop & Shop", TxnKind::Expense},
        {"2026-10-03", "-12.50",   "Dining",        "Chipotle",    TxnKind::Expense},
        {"2026-10-04", "-5.25",    "Coffee",        "Dunkin",      TxnKind::Expense},
        {"2026-10-05", "-15.99",   "Subscriptions", "Spotify",     TxnKind::Expense},
    };
    for (const Sample& s : samples) {
        Transaction t;
        t.userId   = kUserId;
        t.date     = *Date::parse(s.date);
        t.amount   = *Money::parse(s.amount);
        t.category = s.category;
        t.merchant = s.merchant;
        t.kind     = s.kind;
        repo.addTransaction(t);
    }
    std::cout << "Added " << std::size(samples) << " sample transactions." << std::endl;
}

}  // namespace

int main() {
    // Catch the most common mistake early: running from the wrong folder.
    if (!std::filesystem::exists("web/index.html")) {
        std::cerr << "Can't find web/index.html.\n"
                  << "Run this from the project folder, e.g.  .\\build\\huskybudget.exe\n";
        return 1;
    }
    std::filesystem::create_directories("data");

    JsonFileRepository repo("data/huskybudget.json");
    addSampleDataIfEmpty(repo);

    // The server answers several requests at once on different threads.
    // Only one thread may touch the repository at a time, so every handler
    // locks this mutex first.
    std::mutex repoMutex;

    httplib::Server server;
    server.set_mount_point("/", "./web");
    server.set_payload_max_length(5 * 1024 * 1024);    // refuse uploads over 5 MB

    server.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // GET /api/transactions?month=2026-10      (or ?from=&to=, or nothing for all)
    server.Get("/api/transactions", [&](const httplib::Request& req, httplib::Response& res) {
        Date from, to;
        std::string error;
        if (!readDateRange(req, from, to, error)) return sendError(res, 400, error);

        std::vector<Transaction> txns;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            txns = repo.transactionsInRange(kUserId, from, to);
        }

        json out = json::array();
        for (const Transaction& t : txns) out.push_back(toApiJson(t));
        res.set_content(out.dump(), "application/json");
    });

    // POST /api/transactions   body: {"date":..., "amount":..., "kind":..., "category":..., ...}
    server.Post("/api/transactions", [&](const httplib::Request& req, httplib::Response& res) {
        std::string error;
        auto txn = transactionFromRequest(req.body, error);
        if (!txn) return sendError(res, 400, error);

        {
            std::lock_guard<std::mutex> lock(repoMutex);
            txn->id = repo.addTransaction(*txn);
        }

        res.status = 201;   // "Created"
        res.set_content(toApiJson(*txn).dump(), "application/json");
    });

    // DELETE /api/transactions/7
    server.Delete(R"(/api/transactions/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1].str());

        bool removed;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            removed = repo.deleteTransaction(id, kUserId);
        }

        if (!removed) return sendError(res, 404, "no transaction with id " + std::to_string(id));
        res.status = 204;   // "No Content": it worked, nothing to send back
    });

    // POST /api/import     body: the text of a CSV file
    //
    // All or nothing: if any row has a problem, nothing is imported and every
    // problem is reported, so the user can fix the file and try again without
    // ending up with half an import.
    server.Post("/api/import", [&](const httplib::Request& req, httplib::Response& res) {
        ImportResult result = parseCsv(req.body, kUserId);

        if (!result.ok()) {
            json errors = json::array();
            for (const ImportError& e : result.errors) {
                errors.push_back({{"line", e.line}, {"message", e.message}});
            }
            res.status = 400;
            res.set_content(json{{"error", "nothing was imported"}, {"problems", errors}}.dump(),
                            "application/json");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(repoMutex);
            for (const Transaction& t : result.transactions) repo.addTransaction(t);
        }
        res.set_content(json{{"imported", result.transactions.size()}}.dump(), "application/json");
    });

    // GET /api/summary?month=2026-10
    server.Get("/api/summary", [&](const httplib::Request& req, httplib::Response& res) {
        auto month = YearMonth::parse(req.get_param_value("month"));
        if (!month) return sendError(res, 400, "month must look like 2026-10");

        std::vector<Transaction> txns;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            txns = repo.transactionsInRange(kUserId, month->firstDay(), month->lastDay());
        }

        MonthSummary summary = summarize(txns);    // all the math lives in Summary.cpp
        res.set_content(toApiJson(*month, summary).dump(), "application/json");
    });

    // GET /api/budgets?month=2026-10
    // Every budget with how much is spent, what's left, and the end-of-month guess.
    server.Get("/api/budgets", [&](const httplib::Request& req, httplib::Response& res) {
        auto month = YearMonth::parse(req.get_param_value("month"));
        if (!month) return sendError(res, 400, "month must look like 2026-10");

        std::vector<Budget> budgets;
        std::vector<Transaction> txns;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            budgets = repo.budgetsFor(kUserId);
            txns    = repo.transactionsInRange(kUserId, month->firstDay(), month->lastDay());
        }

        json out = json::array();
        for (const BudgetStatus& b : evaluateBudgets(budgets, txns, *month, today())) {
            out.push_back(toApiJson(b));               // all the math lives in BudgetStatus.cpp
        }
        res.set_content(out.dump(), "application/json");
    });

    // PUT /api/budgets     body: {"category":"Coffee", "limit":"60"}
    // Adds a budget, or changes the limit if that category already has one.
    server.Put("/api/budgets", [&](const httplib::Request& req, httplib::Response& res) {
        json j = json::parse(req.body, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.is_object()) return sendError(res, 400, "body must be a JSON object");

        auto text = [&](const char* key) -> std::string {
            if (!j.contains(key) || !j[key].is_string()) return "";
            return j[key].get<std::string>();
        };

        Budget b;
        b.userId   = kUserId;
        b.category = text("category");
        if (b.category.empty()) return sendError(res, 400, "category is required");

        auto limit = Money::parse(text("limit"));
        if (!limit)              return sendError(res, 400, "limit must be a number like 200");
        if (!(Money() < *limit)) return sendError(res, 400, "limit must be more than $0.00");
        b.monthlyLimit = *limit;

        {
            std::lock_guard<std::mutex> lock(repoMutex);
            repo.setBudget(b);
        }
        res.set_content(json{{"category", b.category}, {"limit", moneyJson(b.monthlyLimit)}}.dump(),
                        "application/json");
    });

    // GET /api/alerts?month=2026-10
    // Charges in that month that look wrong. The rules compare each charge to
    // everything before it, so this loads the whole history up to the month's end.
    server.Get("/api/alerts", [&](const httplib::Request& req, httplib::Response& res) {
        auto month = YearMonth::parse(req.get_param_value("month"));
        if (!month) return sendError(res, 400, "month must look like 2026-10");

        std::vector<Transaction> history;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            history = repo.transactionsInRange(kUserId, Date{1900, 1, 1}, month->lastDay());
        }

        json out = json::array();
        for (const Alert& a : findAlerts(history, *month)) {
            out.push_back(toApiJson(a));               // all the rules live in Alerts.cpp
        }
        res.set_content(out.dump(), "application/json");
    });

    // GET /api/score?month=2026-10
    // The 0-100 spending score, how each part was earned, and advice.
    server.Get("/api/score", [&](const httplib::Request& req, httplib::Response& res) {
        auto month = YearMonth::parse(req.get_param_value("month"));
        if (!month) return sendError(res, 400, "month must look like 2026-10");

        std::vector<Transaction> history;
        std::vector<Budget> budgets;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            history = repo.transactionsInRange(kUserId, Date{1900, 1, 1}, month->lastDay());
            budgets = repo.budgetsFor(kUserId);
        }

        SpendingScore score = scoreMonth(history, budgets, *month, today());   // math in Score.cpp
        res.set_content(toApiJson(score).dump(), "application/json");
    });

    // GET /api/whatif?monthly=200&rate=7&years=10
    // "If I saved this much every month, what would it grow to?"
    // Doesn't touch the stored data, so it doesn't need the lock.
    server.Get("/api/whatif", [&](const httplib::Request& req, httplib::Response& res) {
        auto monthly = Money::parse(req.get_param_value("monthly"));
        if (!monthly || !(Money() < *monthly)) {
            return sendError(res, 400, "monthly must be a number like 200");
        }

        double rate = 0;
        if (!parseNumber(req.get_param_value("rate"), rate) || rate < 0 || rate > 30) {
            return sendError(res, 400, "rate must be a number from 0 to 30");
        }

        double years = 0;
        if (!parseNumber(req.get_param_value("years"), years) || years < 1 || years > 50 ||
            years != static_cast<int>(years)) {
            return sendError(res, 400, "years must be a whole number from 1 to 50");
        }

        WhatIfResult result = whatIf(*monthly, rate, static_cast<int>(years));
        res.set_content(toApiJson(result).dump(), "application/json");
    });

    std::cout << "HuskyBudget running at http://localhost:" << kPort << "\n"
              << "Press Ctrl+C to stop." << std::endl;
    if (!server.listen("127.0.0.1", kPort)) {
        std::cerr << "Could not start on port " << kPort
                  << ". Is another copy already running?\n";
        return 1;
    }
    return 0;
}