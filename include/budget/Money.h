#pragma once
#include <cstdint>

namespace budget {

class Money {
public:
    Money() = default;                       // zero dollars

    static Money fromCents(std::int64_t cents);
    std::int64_t cents() const;

private:
    std::int64_t cents_ = 0;
};

}  // namespace budget