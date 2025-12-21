#pragma once

#include "common.h"
#include "formula.h"
#include "sheet.h"

#include <cmath>
#include <functional>
#include <optional>
#include <set>
#include <unordered_set>

class Sheet;

class Cell : public CellInterface {
public:
    friend class Sheet;
    Cell(Sheet& sheet, Position pos);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;

private:
    Sheet& sheet_;
    Position pos_;

    std::string text_;
    std::unique_ptr<FormulaInterface> formula_;

    mutable std::optional<Value> cache_;

    std::set<Cell*> depends_on_;
    std::set<Cell*> depended_by_;

    void InvalidateCache();
};
