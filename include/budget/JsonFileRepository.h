#pragma once
#include <string>

#include "budget/InMemoryRepository.h"

namespace budget {

// Same behavior as InMemoryRepository, but every change is also written to a
// JSON file, and the file is read back when the repository is created.
//
// It inherits from InMemoryRepository, so all the searching and filtering
// code is reused. This class only adds "load from file" and "save to file".
class JsonFileRepository : public InMemoryRepository {
public:
    // Loads the file if it exists. A missing file just means "start empty".
    // Throws std::runtime_error if the file exists but is broken.
    explicit JsonFileRepository(std::string path);

    int  addTransaction(const Transaction& txn) override;
    bool deleteTransaction(int id, int userId) override;
    void setBudget(const Budget& budget) override;

private:
    void load();
    void save() const;

    std::string path_;
};

}  // namespace budget