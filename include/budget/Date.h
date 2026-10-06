#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace budget {

struct Date {
    int year  = 1970;
    int month = 1;     // 1..12
    int day   = 1;     // 1..31

    static std::optional<Date> parse(std::string_view iso);   // "2026-10-05"
    std::string toIso() const;                                 // "2026-10-05"

    static bool isLeapYear(int year);
    static int  daysInMonth(int year, int month);

    bool operator==(const Date& rhs) const;
    bool operator!=(const Date& rhs) const;
    bool operator<(const Date& rhs) const;
};

}  // namespace budget