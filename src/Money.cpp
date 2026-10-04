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
    // true if this one has fewer cents than rhs
    return cents_ < rhs.cents_;
}

}  // namespace budget