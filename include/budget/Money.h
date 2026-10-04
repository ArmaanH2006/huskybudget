#pragma once
#include <cstdint>

namespace budget {

class Money {
public:
    Money() = default;                       // zero dollars

    static Money fromCents(std::int64_t cents);
    std::int64_t cents() const;

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