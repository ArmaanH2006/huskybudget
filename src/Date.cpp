#include "budget/Date.h"

namespace budget {

// Leap year rule: divisible by 4, except centuries, except every 400 years.
//   2024 -> yes   2023 -> no   1900 -> no   2000 -> yes
bool Date::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int Date::daysInMonth(int year, int month) {
    switch (month) {
        case 2:
            return isLeapYear(year) ? 29 : 28;
        case 4: case 6: case 9: case 11:
            return 30;
        default:
            return 31;
    }
}

// parse: exactly "YYYY-MM-DD", digits only, and the date must really exist.
// Rejects "2026-13-01", "2026-02-30", "2026-2-5", "26-10-05", "".
std::optional<Date> Date::parse(std::string_view iso) {
    if (iso.size() != 10 || iso[4] != '-' || iso[7] != '-') {
        return std::nullopt;
    }

    // Reads `count` digits starting at `pos`. Returns -1 if any isn't a digit.
    auto readNumber = [&](std::size_t pos, std::size_t count) {
        int value = 0;
        for (std::size_t k = pos; k < pos + count; ++k) {
            if (iso[k] < '0' || iso[k] > '9') return -1;
            value = value * 10 + (iso[k] - '0');
        }
        return value;
    };

    Date d;
    d.year  = readNumber(0, 4);
    d.month = readNumber(5, 2);
    d.day   = readNumber(8, 2);

    if (d.year < 0 || d.month < 0 || d.day < 0) return std::nullopt;   // non-digit
    if (d.month < 1 || d.month > 12)            return std::nullopt;
    if (d.day < 1 || d.day > daysInMonth(d.year, d.month)) return std::nullopt;

    return d;
}

std::string Date::toIso() const {
    auto pad = [](int value, std::size_t width) {
        std::string s = std::to_string(value);
        while (s.size() < width) s.insert(s.begin(), '0');
        return s;
    };
    return pad(year, 4) + "-" + pad(month, 2) + "-" + pad(day, 2);
}


bool Date::operator==(const Date& rhs) const {
    // true if year, month AND day all match
    return rhs.year == year && rhs.month == month && rhs.day == day;
}

bool Date::operator!=(const Date& rhs) const {
    // the opposite of ==
    return rhs.year != year || rhs.month != month || rhs.day != day;
    
}

bool Date::operator<(const Date& rhs) const {
    // is the date less than that of rhs?
    if (year != rhs.year)   return year < rhs.year;
    if (month != rhs.month) return month < rhs.month;
    return day < rhs.day;
}

}  // namespace budget