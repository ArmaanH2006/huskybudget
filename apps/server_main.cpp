// HuskyBudget web server.
//
// Run it from the project folder (the one containing web/ and data/):
//     .\build\huskybudget.exe
// then open http://localhost:8080 in a browser.

#include <filesystem>
#include <iostream>
#include <iterator>
#include <mutex>
#include <vector>

#include "httplib.h"
#include "json.hpp"

#include "budget/JsonFileRepository.h"

using nlohmann::json;
using namespace budget;

namespace {

constexpr int kUserId = 1;            // single user until login exists
constexpr int kPort   = 8080;

// Turns a Transaction into the JSON the browser receives.
// "amount" is ready to display; "cents" is there for any math the page does.
json toApiJson(const Transaction& t) {
    const char* kind = t.kind == TxnKind::Income   ? "income"
                     : t.kind == TxnKind::Transfer ? "transfer"
                                                   : "expense";
    return {
        {"id",       t.id},
        {"date",     t.date.toIso()},
        {"amount",   t.amount.toString()},
        {"cents",    t.amount.cents()},
        {"category", t.category},
        {"merchant", t.merchant},
        {"note",     t.note},
        {"kind",     kind},
    };
}

// A handful of transactions so the page isn't empty the first time.
// The real 12-month seed generator comes in week 2.
void addSampleDataIfEmpty(Repository& repo) {
    Date everything_from{1900, 1, 1};
    Date everything_to{9999, 12, 31};
    if (!repo.transactionsInRange(kUserId, everything_from, everything_to).empty()) return;

    struct Sample { const char* date; const char* amount; const char* category;
                    const char* merchant; TxnKind kind; };
    const Sample samples[] = {
        {"2026-10-01", "2400.00", "Salary",    "Employer",     TxnKind::Income},
        {"2026-10-01", "-1100.00", "Rent",     "Landlord",     TxnKind::Expense},
        {"2026-10-02", "-64.18",  "Groceries", "Stop & Shop",  TxnKind::Expense},
        {"2026-10-03", "-12.50",  "Dining",    "Chipotle",     TxnKind::Expense},
        {"2026-10-04", "-5.25",   "Coffee",    "Dunkin",       TxnKind::Expense},
        {"2026-10-05", "-15.99",  "Subscriptions", "Spotify",  TxnKind::Expense},
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

    // Serve index.html, app.js, etc. from the web/ folder.
    server.set_mount_point("/", "./web");

    server.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // GET /api/transactions?from=2026-10-01&to=2026-10-31   (both optional)
    server.Get("/api/transactions", [&](const httplib::Request& req, httplib::Response& res) {
        // 1. Read the request.
        Date from{1900, 1, 1};
        Date to{9999, 12, 31};
        if (req.has_param("from")) {
            auto d = Date::parse(req.get_param_value("from"));
            if (!d) { res.status = 400; res.set_content(R"({"error":"bad 'from' date"})", "application/json"); return; }
            from = *d;
        }
        if (req.has_param("to")) {
            auto d = Date::parse(req.get_param_value("to"));
            if (!d) { res.status = 400; res.set_content(R"({"error":"bad 'to' date"})", "application/json"); return; }
            to = *d;
        }

        // 2. Ask the repository.
        std::vector<Transaction> txns;
        {
            std::lock_guard<std::mutex> lock(repoMutex);
            txns = repo.transactionsInRange(kUserId, from, to);
        }

        // 3. Send it back as JSON.
        json out = json::array();
        for (const Transaction& t : txns) out.push_back(toApiJson(t));
        res.set_content(out.dump(), "application/json");
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