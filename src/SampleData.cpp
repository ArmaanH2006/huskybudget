#include "budget/SampleData.h"

#include <random>
#include <string>

namespace budget {

namespace {

// Wraps the random-number engine so the rules below read like a description
// of someone's month instead of a pile of distribution objects.
class Randomizer {
public:
    explicit Randomizer(unsigned seed) : engine_(seed) {}

    int between(int low, int high) {                       // inclusive
        return std::uniform_int_distribution<int>(low, high)(engine_);
    }
    Money cents(int lowCents, int highCents) {
        return Money::fromCents(between(lowCents, highCents));
    }
    template <std::size_t N>
    const char* pick(const char* const (&options)[N]) {
        return options[between(0, static_cast<int>(N) - 1)];
    }

private:
    std::mt19937 engine_;
};

// Collects one month's transactions and drops any dated after `lastDay_`.
class MonthBuilder {
public:
    MonthBuilder(std::vector<Transaction>& out, int userId, int year, int month, Date lastDay)
        : out_(out), userId_(userId), year_(year), month_(month), lastDay_(lastDay) {}

    int daysInMonth() const { return Date::daysInMonth(year_, month_); }

    void spend(int day, Money amount, const char* category, const char* merchant) {
        add(day, -amount, category, merchant, TxnKind::Expense);
    }
    void earn(int day, Money amount, const char* category, const char* merchant) {
        add(day, amount, category, merchant, TxnKind::Income);
    }

private:
    void add(int day, Money amount, const char* category, const char* merchant, TxnKind kind) {
        Date date{year_, month_, day};
        if (lastDay_ < date) return;                       // no future transactions
        Transaction t;
        t.userId   = userId_;
        t.date     = date;
        t.amount   = amount;
        t.category = category;
        t.merchant = merchant;
        t.kind     = kind;
        out_.push_back(t);
    }

    std::vector<Transaction>& out_;
    int  userId_, year_, month_;
    Date lastDay_;
};

}  // namespace

std::vector<Transaction> generateSampleData(Date lastDay, int months, unsigned seed, int userId) {
    std::vector<Transaction> out;
    Randomizer rng(seed);

    // Step back to the first month in the range.
    int year  = lastDay.year;
    int month = lastDay.month - (months - 1);
    while (month < 1) { month += 12; --year; }

    for (int i = 0; i < months; ++i) {
        MonthBuilder m(out, userId, year, month, lastDay);
        const int last = m.daysInMonth();
        const bool winter = (month == 12 || month <= 2);

        // ---- Income: paid on the 1st and 15th
        m.earn(1,  Money::fromCents(120000), "Salary", "Employer");
        m.earn(15, Money::fromCents(120000), "Salary", "Employer");

        // ---- Fixed bills
        m.spend(1,  Money::fromCents(110000),                       "Rent",          "Landlord");
        m.spend(10, winter ? rng.cents(11000, 16000) : rng.cents(5500, 9000), "Utilities", "Eversource");
        m.spend(18, Money::fromCents(4500),                         "Phone",         "Verizon");
        m.spend(5,  Money::fromCents(1199),                         "Subscriptions", "Spotify");
        m.spend(12, Money::fromCents(1549),                         "Subscriptions", "Netflix");

        // ---- Everyday spending
        const char* const grocers[] = {"Stop & Shop", "Trader Joe's", "Aldi"};
        for (int n = rng.between(4, 6); n > 0; --n)
            m.spend(rng.between(1, last), rng.cents(3500, 9500), "Groceries", rng.pick(grocers));

        const char* const restaurants[] = {"Chipotle", "Panera", "Domino's", "Sweetgreen", "Moe's"};
        for (int n = rng.between(5, 10); n > 0; --n)
            m.spend(rng.between(1, last), rng.cents(900, 3800), "Dining", rng.pick(restaurants));

        const char* const cafes[] = {"Dunkin", "Starbucks"};
        for (int n = rng.between(8, 15); n > 0; --n)
            m.spend(rng.between(1, last), rng.cents(350, 750), "Coffee", rng.pick(cafes));

        const char* const gasStations[] = {"Shell", "Mobil"};
        for (int n = rng.between(2, 3); n > 0; --n)
            m.spend(rng.between(1, last), rng.cents(3000, 5200), "Transportation", rng.pick(gasStations));
        if (rng.between(1, 3) == 1)
            m.spend(rng.between(1, last), rng.cents(1200, 2800), "Transportation", "Uber");

        const char* const stores[] = {"Amazon", "Target"};
        for (int n = rng.between(1, 3); n > 0; --n)
            m.spend(rng.between(1, last), rng.cents(1500, 12000), "Shopping", rng.pick(stores));

        const char* const fun[] = {"AMC Theatres", "Steam", "Bowlero"};
        for (int n = rng.between(0, 2); n > 0; --n)
            m.spend(rng.between(1, last), rng.cents(1000, 6000), "Entertainment", rng.pick(fun));

        if (rng.between(1, 2) == 1)
            m.spend(rng.between(1, last), rng.cents(800, 4500), "Health", "CVS");

        // ---- Deliberate oddities, so the alerts have something to find.
        // They go in the last full month, so a fresh demo always shows them.
        if (month == 12) {
            for (int n = 4; n > 0; --n)                                       // holiday gifts
                m.spend(rng.between(5, 22), rng.cents(2500, 9000), "Shopping", rng.pick(stores));
        }
        if (i == months - 2) {
            m.spend(28, Money::fromCents(64999), "Shopping", "Best Buy");     // a big new TV
            Money charge = rng.cents(1800, 3200);                             // charged twice
            int day = rng.between(3, 25);
            m.spend(day, charge, "Dining", "Chipotle");
            m.spend(day, charge, "Dining", "Chipotle");
        }

        if (++month > 12) { month = 1; ++year; }
    }
    return out;
}

}  // namespace budget