#include "budget/Money.h"

namespace budget {

Money Money::fromCents(std::int64_t cents) {
    Money m;
    m.cents_ = cents;
    return m;
}

std::int64_t Money::cents() const {
    return cents_;
}

}  // namespace budget