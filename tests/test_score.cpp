#include "doctest.h"
#include "budget/Score.h"

using namespace budget;

namespace {

const YearMonth kOctober{2026, 10};
const Date      kAfterOctober{2026, 11, 15};    // October is finished

// A list of transactions, oldest first, like the repository returns.
// Merchants are left blank unless a test needs them, so no alerts fire by accident.
struct Book {
    std::vector<Transaction> txns;

    void earn(Date date, const char* amount) {
        Transaction t;
        t.date = date; t.amount = *Money::parse(amount);
        t.category = "Salary"; t.kind = TxnKind::Income;
        txns.push_back(t);
    }
    void spend(Date date, const char* amount, const char* category, const char* merchant = "") {
        Transaction t;
        t.date = date; t.amount = -*Money::parse(amount);
        t.category = category; t.merchant = merchant; t.kind = TxnKind::Expense;
        txns.push_back(t);
    }
};

Budget makeBudget(const char* category, const char* limit) {
    return Budget{1, category, *Money::parse(limit)};
}

SpendingScore scoreOctober(const Book& book, const std::vector<Budget>& budgets = {}) {
    return scoreMonth(book.txns, budgets, kOctober, kAfterOctober);
}

// The parts always come in this order.
enum Part { Savings, Budgets, Steady, Wants, Surprises };

}  // namespace

TEST_CASE("letter grades") {
    CHECK(gradeFor(100) == "A");
    CHECK(gradeFor(90)  == "A");
    CHECK(gradeFor(89)  == "B");
    CHECK(gradeFor(80)  == "B");
    CHECK(gradeFor(79)  == "C");
    CHECK(gradeFor(60)  == "D");
    CHECK(gradeFor(59)  == "F");
    CHECK(gradeFor(0)   == "F");
}

TEST_CASE("the five parts add up to 100 points") {
    SpendingScore s = scoreOctober(Book{});
    REQUIRE(s.parts.size() == 5);
    int max = 0;
    for (const ScorePart& p : s.parts) max += p.maxPoints;
    CHECK(max == 100);
}

TEST_CASE("savings: keeping 20% of income gets all 30 points") {
    Book b;
    b.earn(Date{2026, 10, 1}, "1000.00");
    b.spend(Date{2026, 10, 2}, "800.00", "Rent");
    SpendingScore s = scoreOctober(b);
    CHECK(s.parts[Savings].points == 30);
    CHECK(s.parts[Savings].detail == "You kept 20% of your income (goal: 20%)");
}

TEST_CASE("savings: keeping 10% gets half, spending more than you earn gets none") {
    Book half;
    half.earn(Date{2026, 10, 1}, "1000.00");
    half.spend(Date{2026, 10, 2}, "900.00", "Rent");
    CHECK(scoreOctober(half).parts[Savings].points == 15);

    Book overspent;
    overspent.earn(Date{2026, 10, 1}, "1000.00");
    overspent.spend(Date{2026, 10, 2}, "1100.00", "Rent");
    CHECK(scoreOctober(overspent).parts[Savings].points == 0);
    CHECK(scoreOctober(overspent).parts[Savings].detail == "You spent 10% more than you earned (goal: keep 20%)");
    CHECK(scoreOctober(overspent).tips[0] == "You spent more than you earned. Saving $300.00 more would reach the 20% goal.");

    Book noIncome;
    noIncome.spend(Date{2026, 10, 2}, "100.00", "Rent");
    CHECK(scoreOctober(noIncome).parts[Savings].points == 0);
    CHECK(scoreOctober(noIncome).parts[Savings].detail == "No income recorded this month");
}

TEST_CASE("budgets: points for each budget kept under the limit") {
    Book b;
    b.earn(Date{2026, 10, 1}, "3000.00");
    b.spend(Date{2026, 10, 5}, "300.00", "Groceries");   // under $400
    b.spend(Date{2026, 10, 6}, "300.00", "Dining");      // over $250

    SpendingScore s = scoreOctober(b, {makeBudget("Groceries", "400"), makeBudget("Dining", "250")});
    CHECK(s.parts[Budgets].points == 13);                 // 1 of 2 = 12.5, rounded
    CHECK(s.parts[Budgets].detail == "1 of 2 budgets stayed under the limit");

    bool hasTip = false;
    for (const std::string& tip : s.tips) hasTip |= (tip == "You went over your Dining budget by $50.00.");
    CHECK(hasTip);
}

TEST_CASE("budgets: none set means no points, and a tip to set some") {
    SpendingScore s = scoreOctober(Book{});
    CHECK(s.parts[Budgets].points == 0);
    CHECK(s.tips.back() == "Set budgets for your biggest categories so the app can warn you early.");
}

TEST_CASE("steady spending: compared with the last 3 months") {
    Book past;
    for (int month = 7; month <= 9; ++month) past.spend(Date{2026, month, 3}, "1000.00", "Rent");

    Book same = past;
    same.spend(Date{2026, 10, 3}, "1000.00", "Rent");
    CHECK(scoreOctober(same).parts[Steady].points == 20);

    Book more = past;
    more.spend(Date{2026, 10, 3}, "1100.00", "Rent");     // 10% more: lose 40% of the points
    CHECK(scoreOctober(more).parts[Steady].points == 12);
    CHECK(scoreOctober(more).parts[Steady].detail == "Spending is 10% above your recent average");

    Book muchMore = past;
    muchMore.spend(Date{2026, 10, 3}, "1250.00", "Rent");  // 25% more: no points
    CHECK(scoreOctober(muchMore).parts[Steady].points == 0);

    Book less = past;
    less.spend(Date{2026, 10, 3}, "700.00", "Rent");
    CHECK(scoreOctober(less).parts[Steady].points == 20);
    CHECK(scoreOctober(less).parts[Steady].detail == "Spending is 30% below your recent average");
}

TEST_CASE("steady spending: a month in progress is compared fairly") {
    Book b;
    for (int month = 7; month <= 9; ++month) b.spend(Date{2026, month, 3}, "310.00", "Groceries");
    b.spend(Date{2026, 10, 3}, "100.00", "Groceries");

    // On Oct 10, a normal month would have about 10/31 of $310 = $100 spent.
    SpendingScore s = scoreMonth(b.txns, {}, kOctober, Date{2026, 10, 10});
    CHECK(s.parts[Steady].points == 20);

    // On Oct 3, it's too early to say.
    SpendingScore early = scoreMonth(b.txns, {}, kOctober, Date{2026, 10, 3});
    CHECK(early.parts[Steady].detail == "Too early in the month to compare");
}

TEST_CASE("steady spending: no history yet means no penalty") {
    Book b;
    b.spend(Date{2026, 10, 3}, "5000.00", "Rent");
    SpendingScore s = scoreOctober(b);
    CHECK(s.parts[Steady].points == 20);
    CHECK(s.parts[Steady].detail == "Not enough history to compare yet");
}

TEST_CASE("needs vs wants: 30% or less on wants is full points, 50% is none") {
    auto wantsPoints = [](const char* rent, const char* dining) {
        Book b;
        b.spend(Date{2026, 10, 1}, rent, "Rent");
        b.spend(Date{2026, 10, 2}, dining, "Dining");
        return scoreOctober(b).parts[Wants].points;
    };
    CHECK(wantsPoints("700.00", "300.00") == 15);   // 30%
    CHECK(wantsPoints("650.00", "350.00") == 11);   // 35%: a quarter of the way, 11.25 rounded
    CHECK(wantsPoints("500.00", "500.00") == 0);    // 50%
}

TEST_CASE("needs vs wants: tip names the biggest want") {
    Book b;
    b.earn(Date{2026, 10, 1}, "5000.00");
    b.spend(Date{2026, 10, 1}, "500.00", "Rent");
    b.spend(Date{2026, 10, 2}, "400.00", "Shopping");
    b.spend(Date{2026, 10, 3}, "100.00", "Coffee");
    SpendingScore s = scoreOctober(b);

    bool hasTip = false;
    for (const std::string& tip : s.tips) {
        hasTip |= (tip == "Shopping was your biggest want at $400.00. "
                          "Cutting it by a quarter would free up about $100.00 a month.");
    }
    CHECK(hasTip);
}

TEST_CASE("no surprises: lose 5 points per alert") {
    Book b;
    b.spend(Date{2026, 10, 9}, "19.02", "Dining", "Chipotle");
    b.spend(Date{2026, 10, 9}, "19.02", "Dining", "Chipotle");     // double charge
    SpendingScore s = scoreOctober(b);
    CHECK(s.parts[Surprises].points == 5);
    CHECK(s.parts[Surprises].detail == "1 alert(s) this month");
}

TEST_CASE("a healthy month: high score, grade A, and a nice-month tip") {
    Book b;
    for (int month = 7; month <= 10; ++month) {
        b.earn(Date{2026, month, 1}, "3000.00");
        b.spend(Date{2026, month, 2}, "1100.00", "Rent");
        b.spend(Date{2026, month, 9}, "300.00", "Groceries");
        b.spend(Date{2026, month, 20}, "150.00", "Dining");
    }
    SpendingScore s = scoreOctober(b, {makeBudget("Groceries", "400"), makeBudget("Dining", "250")});
    CHECK(s.total == 100);
    CHECK(s.grade == "A");
    REQUIRE(s.tips.size() == 1);
    CHECK(s.tips[0] == "Nice month. Everything looks healthy.");
}