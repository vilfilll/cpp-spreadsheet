#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() = default;

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) throw InvalidPositionException("Invalid position");

    Cell* cell = GetConcreteCell(pos);
    if (!cell) {
        Cell temp(*this, pos);
        temp.Set(text);  
        CheckCircularDependency(temp, temp.GetReferencedCells());

        auto& real = cells_[pos];
        real = std::make_unique<Cell>(*this, pos);
        real->Set(std::move(text));
        return;
    }

    if (cell->GetText() == text) {
        return;
    }

    std::string old_text = cell->GetText();

    try {
        cell->Set(text);
        CheckCircularDependency(*cell, cell->GetReferencedCells());
    } catch (...) {
        cell->Set(old_text);
        throw;
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

CellInterface* Sheet::GetCell(Position pos) {
    return const_cast<CellInterface*>(
        static_cast<const Sheet&>(*this).GetCell(pos)
    );
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position");
    }

    auto it = cells_.find(pos);
    if (it == cells_.end()) {
        return;
    }

    it->second->Clear();
    cells_.erase(it);
}

Size Sheet::GetPrintableSize() const {
    int max_row = -1;
    int max_col = -1;

    for (const auto& [pos, cell] : cells_) {
        auto val = cell->GetValue();
        bool is_nonempty = false;
        std::visit([&is_nonempty](auto&& arg){
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, double>) {
                is_nonempty = true; 
            } else if constexpr (std::is_same_v<T, std::string>) {
                is_nonempty = !arg.empty();
            } else { 
                is_nonempty = true;
            }
        }, val);

        if (is_nonempty) {
            max_row = std::max(max_row, pos.row);
            max_col = std::max(max_col, pos.col);
        }
    }

    return {
        max_row + 1,
        max_col + 1
    };
}

void Sheet::PrintValues(std::ostream& output) const {
    Size size = GetPrintableSize();
    for (int r = 0; r < size.rows; ++r) {
        for (int c = 0; c < size.cols; ++c) {
            if (c > 0) output << '\t';

            auto cell = GetConcreteCell({r, c});
            if (!cell) {
                output << "";
            } else {
                auto val = cell->GetValue();
                std::visit([&output](auto&& arg){
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, double>) {
                        output << arg;
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        output << arg;
                    } else {
                        output << arg.ToString();
                    }
                }, val);
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    Size size = GetPrintableSize();
    for (int r = 0; r < size.rows; ++r) {
        for (int c = 0; c < size.cols; ++c) {
            if (c > 0) output << '\t';

            auto cell = GetConcreteCell({r, c});
            if (!cell) {
                output << "";
            } else {
                output << cell->GetText();
            }
        }
        output << '\n';
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}

void Sheet::CheckCircularDependency(Cell& start,
                                    const std::vector<Position>& refs) {
    std::set<Cell*> visited;
    std::set<Cell*> stack;

    std::function<void(Cell*)> dfs = [&](Cell* cell) {
        if (stack.count(cell)) {
            throw CircularDependencyException("cycle");
        }
        if (!visited.insert(cell).second) {
            return;
        }

        stack.insert(cell);
        for (Cell* dep : cell->depends_on_) {
            dfs(dep);
        }
        stack.erase(cell);
    };

    std::set<Cell*> temp;
    for (Position pos : refs) {
        if (Cell* dep = GetConcreteCell(pos)) {
            temp.insert(dep);
        }
    }

    auto old = start.depends_on_;
    start.depends_on_ = temp;

    try {
        dfs(&start);
    } catch (...) {
        start.depends_on_ = std::move(old);
        throw;
    }

    start.depends_on_ = std::move(old);
}

Cell* Sheet::GetConcreteCell(Position pos) {
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

const Cell* Sheet::GetConcreteCell(Position pos) const {
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}
