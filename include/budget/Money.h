#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace budget {

class Money {
public:
    Money() = default;                       // zero dollars

    static Money fromCents(std::int64_t cents);
    static std::optional<Money> parse(std::string_view text);  // "$1,234.56"

    std::int64_t cents() const;
    std::string  toString() const;           // "-$1,234.56"

    Money operator+(Money rhs) const;
    Money operator-(Money rhs) const;
    Money operator-() const;                 // unary minus: -m
    bool  operator==(Money rhs) const;
    bool  operator!=(Money rhs) const;
    bool  operator<(Money rhs) const;

private:
    std::int64_t cents_ = 0;
};

}  // namespace budget