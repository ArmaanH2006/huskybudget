#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "budget/Date.h"
#include "budget/Transaction.h"

namespace budget {

// One problem found in the file, with the line it's on (1 = header row).
struct ImportError {
    int         line = 0;
    std::string message;
};

struct ImportResult {
    std::vector<Transaction> transactions;   // every row that parsed
    std::vector<ImportError> errors;         // every row that didn't
    bool ok() const { return errors.empty(); }
};

// Reads a bank-style CSV export. The first row must be a header that names
// the columns; their order doesn't matter and case is ignored.
//
//   Required: Date, Amount            (Amount: negative = money out)
//   Optional: Category                (missing -> "Uncategorized")
//             Merchant / Description / Payee / Name
//             Note / Memo
//
// Dates may be 2026-10-07 or 10/07/2026. Amounts may be "-12.50",
// "$1,234.56", etc. Quoted fields may contain commas and "" for a quote.
ImportResult parseCsv(std::string_view text, int userId);

// Splits one CSV line into fields, handling quotes. Exposed for testing.
std::vector<std::string> splitCsvLine(std::string_view line);

// Accepts "2026-10-07" or "10/7/2026". Exposed for testing.
std::optional<Date> parseFlexibleDate(std::string_view text);

}  // namespace budget