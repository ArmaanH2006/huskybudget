#include "budget/CsvImporter.h"

#include <cctype>
#include <initializer_list>

#include "budget/Money.h"

namespace budget {

namespace {

std::string trim(std::string_view s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Which column holds what. -1 means "this file doesn't have it".
struct Columns {
    int date = -1, amount = -1, category = -1, merchant = -1, note = -1;
};

Columns findColumns(const std::vector<std::string>& header) {
    // Different banks call the same thing different names.
    auto is = [](const std::string& name, std::initializer_list<const char*> options) {
        for (const char* option : options) if (name == option) return true;
        return false;
    };

    Columns cols;
    for (int i = 0; i < static_cast<int>(header.size()); ++i) {
        std::string name = lower(trim(header[i]));
        if      (cols.date == -1     && is(name, {"date", "transaction date", "posted date"}))       cols.date = i;
        else if (cols.amount == -1   && is(name, {"amount"}))                                         cols.amount = i;
        else if (cols.category == -1 && is(name, {"category"}))                                       cols.category = i;
        else if (cols.merchant == -1 && is(name, {"merchant", "description", "payee", "name"}))      cols.merchant = i;
        else if (cols.note == -1     && is(name, {"note", "notes", "memo"}))                          cols.note = i;
    }
    return cols;
}

// Reads a whole number from exactly the characters in `s`. -1 if not digits.
int toNumber(std::string_view s) {
    if (s.empty() || s.size() > 4) return -1;
    int value = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return -1;
        value = value * 10 + (c - '0');
    }
    return value;
}

}  // namespace

std::vector<std::string> splitCsvLine(std::string_view line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') { current += '"'; ++i; }  // "" -> "
            else if (c == '"') inQuotes = false;
            else current += c;
        } else {
            if (c == '"')      inQuotes = true;
            else if (c == ',') { fields.push_back(current); current.clear(); }
            else               current += c;
        }
    }
    fields.push_back(current);
    return fields;
}

std::optional<Date> parseFlexibleDate(std::string_view text) {
    std::string s = trim(text);
    if (auto iso = Date::parse(s)) return iso;              // 2026-10-07

    // M/D/YYYY, the way US banks write it.
    auto slash1 = s.find('/');
    auto slash2 = s.find('/', slash1 == std::string::npos ? 0 : slash1 + 1);
    if (slash1 == std::string::npos || slash2 == std::string::npos) return std::nullopt;

    int month = toNumber(std::string_view(s).substr(0, slash1));
    int day   = toNumber(std::string_view(s).substr(slash1 + 1, slash2 - slash1 - 1));
    std::string_view yearText = std::string_view(s).substr(slash2 + 1);
    if (yearText.size() != 4) return std::nullopt;
    int year = toNumber(yearText);
    if (month < 1 || day < 1 || year < 0) return std::nullopt;

    // Let Date do the real validation (month range, Feb 30, leap years...).
    Date d{year, month, day};
    return Date::parse(d.toIso());
}

ImportResult parseCsv(std::string_view text, int userId) {
    ImportResult result;

    // Skip a UTF-8 "byte order mark" that Excel likes to put at the start.
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);

    // Split into lines, accepting both \n and \r\n endings.
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        start = end + 1;
    }

    // ---- Header
    std::size_t first = 0;
    while (first < lines.size() && trim(lines[first]).empty()) ++first;
    if (first == lines.size()) {
        result.errors.push_back({1, "the file is empty"});
        return result;
    }
    Columns cols = findColumns(splitCsvLine(lines[first]));
    if (cols.date == -1 || cols.amount == -1) {
        result.errors.push_back({static_cast<int>(first) + 1,
                                 "the first row must name the columns, including Date and Amount"});
        return result;
    }

    // ---- Rows
    for (std::size_t i = first + 1; i < lines.size(); ++i) {
        if (trim(lines[i]).empty()) continue;                // ignore blank lines
        const int lineNo = static_cast<int>(i) + 1;
        auto fields = splitCsvLine(lines[i]);

        auto field = [&](int col) -> std::string {
            return (col >= 0 && col < static_cast<int>(fields.size())) ? trim(fields[col]) : "";
        };

        auto date = parseFlexibleDate(field(cols.date));
        if (!date) {
            result.errors.push_back({lineNo, "bad date \"" + field(cols.date) + "\""});
            continue;
        }
        auto amount = Money::parse(field(cols.amount));
        if (!amount) {
            result.errors.push_back({lineNo, "bad amount \"" + field(cols.amount) + "\""});
            continue;
        }
        if (*amount == Money()) {
            result.errors.push_back({lineNo, "amount is zero"});
            continue;
        }

        Transaction t;
        t.userId   = userId;
        t.date     = *date;
        t.amount   = *amount;
        t.kind     = (Money() < *amount) ? TxnKind::Income : TxnKind::Expense;
        t.category = field(cols.category).empty() ? "Uncategorized" : field(cols.category);
        t.merchant = field(cols.merchant);
        t.note     = field(cols.note);
        result.transactions.push_back(t);
    }
    return result;
}

}  // namespace budget