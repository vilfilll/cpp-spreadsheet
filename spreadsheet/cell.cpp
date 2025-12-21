#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <utility>

Cell::Cell(Sheet& sheet, Position pos)
    : sheet_(sheet)
    , pos_(pos) {
}

Cell::~Cell() = default;

void Cell::Clear() {
    text_.clear();
    formula_.reset();
    InvalidateCache();
}

void Cell::Set(std::string text) {
    std::unique_ptr<FormulaInterface> new_formula;
    std::vector<Position> refs;

    if (!text.empty() && text[0] == '=') {
        new_formula = ParseFormula(text.substr(1));
        refs = new_formula->GetReferencedCells();
    }

    sheet_.CheckCircularDependency(*this, refs);

    for (Cell* dep : depends_on_) {
        dep->depended_by_.erase(this);
    }
    depends_on_.clear();

    for (Position pos : refs) {
        Cell* dep = sheet_.GetConcreteCell(pos);
        if (!dep) {
            sheet_.SetCell(pos, "");
            dep = sheet_.GetConcreteCell(pos);
        }
        depends_on_.insert(dep);
        dep->depended_by_.insert(this);
    }

    text_ = std::move(text);
    formula_ = std::move(new_formula);

    InvalidateCache();
}

Cell::Value Cell::GetValue() const {
    if (cache_) {
        return *cache_;
    }

    if (!formula_) {
        if (text_.empty()) {
            cache_ = 0.0;
        } else if (text_[0] == '\'') { 
            cache_ = text_.substr(1);
        } else {
            cache_ = text_;
        }
        return *cache_;
    }

    auto formula_value = formula_->Evaluate(sheet_);

    if (std::holds_alternative<double>(formula_value)) {
        double val = std::get<double>(formula_value);
        if (!std::isfinite(val)) {
            return FormulaError(FormulaError::Category::Arithmetic);
        }
        return val;
    }

    return std::get<FormulaError>(formula_value);
}

std::string Cell::GetText() const {
    if (formula_) {
        return "=" + formula_->GetExpression();
    }
    return text_;
}

std::vector<Position> Cell::GetReferencedCells() const {
    if (!formula_) {
        return {};
    }
    return formula_->GetReferencedCells();
}

void Cell::InvalidateCache() {
    if (!cache_) {
        return;
    }

    cache_.reset();

    for (Cell* parent : depended_by_) {
        parent->InvalidateCache();
    }
}

