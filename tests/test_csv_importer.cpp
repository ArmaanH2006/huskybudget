#include "doctest.h"
#include "budget/CsvImporter.h"

using namespace budget;

TEST_CASE("splitCsvLine handles plain, quoted, and empty fields") {
    auto f = splitCsvLine("a,b,c");
    REQUIRE(f.size() == 3);
    CHECK(f[1] == "b");

    f = splitCsvLine(R"(2026-10-07,"Stop & Shop, Inc",-64.18)");
    REQUIRE(f.size() == 3);
    CHECK(f[1] == "Stop & Shop, Inc");                 // comma inside quotes stays

    f = splitCsvLine(R"(x,"She said ""hi""",y)");
    REQUIRE(f.size() == 3);
    CHECK(f[1] == R"(She said "hi")");                  // "" becomes "

    f = splitCsvLine("a,,c,");
    REQUIRE(f.size() == 4);
    CHECK(f[1].empty());
    CHECK(f[3].empty());
}

TEST_CASE("parseFlexibleDate accepts ISO and US formats, and validates") {
    CHECK(parseFlexibleDate("2026-10-07") == Date{2026, 10, 7});
    CHECK(parseFlexibleDate("10/07/2026") == Date{2026, 10, 7});
    CHECK(parseFlexibleDate("1/5/2026")   == Date{2026, 1, 5});
    CHECK(parseFlexibleDate(" 2/29/2024 ") == Date{2024, 2, 29});   // leap day, with spaces

    for (const char* bad : {"", "2/30/2026", "13/01/2026", "10/07/26", "10-07-2026", "abc", "1//2026"}) {
        INFO("input: ", bad);
        CHECK_FALSE(parseFlexibleDate(bad).has_value());
    }
}

TEST_CASE("a typical bank export") {
    const char* csv =
        "Date,Description,Amount,Category\r\n"
        "10/01/2026,Employer Payroll,\"1,200.00\",Salary\r\n"
        "10/02/2026,\"Stop & Shop, Inc\",-64.18,Groceries\r\n"
        "10/03/2026,Chipotle,$-12.50,Dining\r\n";
    ImportResult r = parseCsv(csv, 7);

    REQUIRE(r.ok());
    REQUIRE(r.transactions.size() == 3);

    CHECK(r.transactions[0].kind == TxnKind::Income);
    CHECK(r.transactions[0].amount.toString() == "$1,200.00");
    CHECK(r.transactions[0].userId == 7);

    CHECK(r.transactions[1].merchant == "Stop & Shop, Inc");
    CHECK(r.transactions[1].amount.cents() == -6418);
    CHECK(r.transactions[1].kind == TxnKind::Expense);

    CHECK(r.transactions[2].date == Date{2026, 10, 3});
    CHECK(r.transactions[2].category == "Dining");
}

TEST_CASE("column order and header names are flexible") {
    const char* csv =
        "Memo,AMOUNT,Payee,Transaction Date\n"
        "lunch,-9.99,Panera,2026-10-04\n";
    ImportResult r = parseCsv(csv, 1);
    REQUIRE(r.ok());
    REQUIRE(r.transactions.size() == 1);
    CHECK(r.transactions[0].merchant == "Panera");
    CHECK(r.transactions[0].note == "lunch");
    CHECK(r.transactions[0].category == "Uncategorized");   // no Category column
    CHECK(r.transactions[0].date == Date{2026, 10, 4});
}

TEST_CASE("bad rows are reported with their line numbers") {
    const char* csv =
        "Date,Amount,Category\n"
        "2026-10-01,-5.00,Coffee\n"       // line 2: fine
        "2026-02-30,-5.00,Coffee\n"       // line 3: no Feb 30
        "2026-10-02,abc,Coffee\n"         // line 4: bad amount
        "\n"                              // line 5: blank, ignored
        "2026-10-03,0.00,Coffee\n";       // line 6: zero
    ImportResult r = parseCsv(csv, 1);

    CHECK_FALSE(r.ok());
    CHECK(r.transactions.size() == 1);
    REQUIRE(r.errors.size() == 3);
    CHECK(r.errors[0].line == 3);
    CHECK(r.errors[1].line == 4);
    CHECK(r.errors[2].line == 6);
}

TEST_CASE("missing required columns or empty file") {
    CHECK_FALSE(parseCsv("", 1).ok());
    CHECK_FALSE(parseCsv("\n\n", 1).ok());

    ImportResult r = parseCsv("Description,Category\nChipotle,Dining\n", 1);
    REQUIRE(r.errors.size() == 1);
    CHECK(r.errors[0].line == 1);
    CHECK(r.transactions.empty());
}

TEST_CASE("Excel's byte-order mark is ignored") {
    ImportResult r = parseCsv("\xEF\xBB\xBF" "Date,Amount\n2026-10-01,-1.00\n", 1);
    CHECK(r.ok());
    CHECK(r.transactions.size() == 1);
}