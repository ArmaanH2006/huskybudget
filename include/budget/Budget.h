#pragma once
#include <string>

#include "budget/Money.h"

namespace budget {

struct Budget {
    int         userId = 0;
    std::string category;        // "Dining"
    Money       monthlyLimit;    // e.g. $200.00
};

}  // namespace budget