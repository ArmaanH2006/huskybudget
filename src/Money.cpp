#include "budget/Money.h"

#include <cctype>

namespace budget {

Money Money::fromCents(std::int64_t cents) {
    Money m;
    m.cents_ = cents;
    return m;
}

std::int64_t Money::cents() const {
    return cents_;
}

Money Money::operator+(Money rhs) const {
    return fromCents(cents_ + rhs.cents_);
}

Money Money::operator-(Money rhs) const {
    return fromCents(cents_ - rhs.cents_);
}

Money Money::operator-() const {
    return fromCents(-cents_);
}

bool Money::operator==(Money rhs) const {
    return rhs.cents_ == cents_;
}

bool Money::operator!=(Money rhs) const {
    return rhs.cents_ != cents_;
}

bool Money::operator<(Money rhs) const {
    return cents_ < rhs.cents_;
}

// ---------------------------------------------------------------------------
// toString: 123456 -> "$1,234.56",  -5 -> "-$0.05"
// ---------------------------------------------------------------------------
std::string Money::toString() const {
    const bool negative = cents_ < 0;

    // Work with the size of the number, ignoring the sign. Doing this in
    // unsigned math means even the most negative int64 can't overflow.
    std::uint64_t magnitude = negative
        ? 0ULL - static_cast<std::uint64_t>(cents_)
        : static_cast<std::uint64_t>(cents_);

    std::uint64_t dollars   = magnitude / 100;   // 123456 / 100 = 1234
    std::uint64_t remainder = magnitude % 100;   // 123456 % 100 = 56

    // Dollars with a comma every three digits, counting from the right.
    std::string digits = std::to_string(dollars);     // "1234"
    std::string withCommas;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count > 0 && count % 3 == 0) {
            withCommas.insert(withCommas.begin(), ',');
        }
        withCommas.insert(withCommas.begin(), *it);
        ++count;
    }

    // Cents always two digits: 5 -> "05", not "5".
    std::string centsPart = (remainder < 10 ? "0" : "") + std::to_string(remainder);

    return (negative ? "-$" : "$") + withCommas + "." + centsPart;
}

// ---------------------------------------------------------------------------
// parse: reads the decimal digits the user typed. Never goes through double,
// so "1.005" really is 1.005 and rounds to 101 cents.
// Returns std::nullopt for anything malformed.
// ---------------------------------------------------------------------------
std::optional<Money> Money::parse(std::string_view text) {
    std::size_t i = 0;
    std::size_t n = text.size();

    // 1. Trim whitespace from both ends.
    while (i < n && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    while (n > i && std::isspace(static_cast<unsigned char>(text[n - 1]))) --n;
    if (i >= n) return std::nullopt;

    // 2. Optional sign and '$', in either order, each at most once.
    bool negative = false;
    bool seenSign = false;
    bool seenCurrency = false;
    while (i < n) {
        if (text[i] == '-' || text[i] == '+') {
            if (seenSign) return std::nullopt;
            seenSign = true;
            negative = (text[i] == '-');
            ++i;
        } else if (text[i] == '$') {
            if (seenCurrency) return std::nullopt;
            seenCurrency = true;
            ++i;
        } else {
            break;
        }
    }

    // 3. Whole-dollar digits, commas allowed between them.
    std::int64_t dollars = 0;
    int dollarDigits = 0;
    while (i < n && (std::isdigit(static_cast<unsigned char>(text[i])) || text[i] == ',')) {
        if (text[i] == ',') {
            if (dollarDigits == 0) return std::nullopt;   // ",5" is malformed
        } else {
            dollars = dollars * 10 + (text[i] - '0');
            if (++dollarDigits > 15) return std::nullopt; // overflow guard
        }
        ++i;
    }

    // 4. Optional '.' followed by at least one digit.
    std::string fraction;
    if (i < n && text[i] == '.') {
        ++i;
        while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) {
            fraction += text[i];
            ++i;
        }
        if (fraction.empty()) return std::nullopt;        // "5." is malformed
    }

    // 5. Anything left over means garbage: "12a", "1.2.3".
    if (i != n) return std::nullopt;

    // 6. Must have seen at least one digit somewhere: rejects "$", ".", "-".
    if (dollarDigits == 0 && fraction.empty()) return std::nullopt;

    // 7. Build the cents. Pad fraction to 2 digits, then round half up on
    //    the third digit. Adding 1 to the combined total handles "9.999"
    //    (999 -> 1000) with no carry logic.
    std::string firstTwo = (fraction + "00").substr(0, 2);
    std::int64_t cents = dollars * 100 + std::stoi(firstTwo);
    if (fraction.size() >= 3 && fraction[2] >= '5') {
        cents += 1;
    }

    return fromCents(negative ? -cents : cents);
}

}  // namespace budget