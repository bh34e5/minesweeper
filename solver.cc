#pragma once

#include "dlist.cc"
#include "grid.cc"

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>

struct SolveState {
    size_t row;
    size_t col;
    size_t rule;
    bool did_epoch_work;
    bool invalid;
};

struct GridSolver {
    struct Rule {
        typedef auto(Apply)(Grid grid, size_t row, size_t col, void *data)
            -> bool;
        typedef auto(OnEpochStart)(Grid grid, void *data) -> void;
        typedef auto(OnEpochFinish)(Grid grid, void *data) -> void;

        Apply *apply;
        OnEpochStart *onStart;
        OnEpochFinish *onFinish;
        void *data;

        Rule()
            : apply(nullptr), onStart(nullptr), onFinish(nullptr),
              data(nullptr) {}
        Rule(Apply *apply)
            : apply(apply), onStart(nullptr), onFinish(nullptr), data(nullptr) {
        }
        Rule(Apply *apply, void *data)
            : apply(apply), onStart(nullptr), onFinish(nullptr), data(data) {}
        Rule(Apply *apply, OnEpochStart *onStart, OnEpochFinish *onFinish,
             void *data)
            : apply(apply), onStart(onStart), onFinish(onFinish), data(data) {}

        auto applyRule(Grid grid, size_t row, size_t col) -> bool {
            if (this->apply == nullptr) {
                return false;
            }
            return this->apply(grid, row, col, this->data);
        }

        auto onEpochStart(Grid grid) -> void {
            if (this->onStart == nullptr) {
                return;
            }
            this->onStart(grid, this->data);
        }

        auto onEpochFinish(Grid grid) -> void {
            if (this->onFinish == nullptr) {
                return;
            }
            this->onFinish(grid, this->data);
        }
    };

    DList<Rule> rules;
    SolveState state;

    auto registerRule(Rule rule) -> void {
        this->rules.push(rule);
        this->state.invalid = true;
    }

    auto step(Grid &grid) -> bool {
        bool did_work = false;
        bool should_continue = true;
        while (!did_work && should_continue) {
            should_continue = this->stepInner(grid, did_work);
        }
        return did_work;
    }

    auto stepInner(Grid &grid, bool &did_work) -> bool {
        if (this->state.invalid) {
            this->resetEpoch(grid);

            this->state.row = 0;
            this->state.col = 0;
            this->state.rule = 0;
            this->state.did_epoch_work = false;
            this->state.invalid = false;
        } else if (this->state.row == 0 && this->state.col == 0 &&
                   this->state.rule == 0) {
            for (auto &rule : this->rules.slice()) {
                rule.onEpochStart(grid);
            }

            this->state.did_epoch_work = false;
        }

        Rule &rule = this->rules.at(this->state.rule++);
        did_work = rule.applyRule(grid, this->state.row, this->state.col);

        if (did_work) {
            this->state.did_epoch_work = true;
        }

        bool has_next_step = true;
        if (this->state.rule == this->rules.len) {
            this->state.rule = 0;
            ++this->state.col;

            if (this->state.col == grid.dims.width) {
                this->state.col = 0;
                ++this->state.row;

                if (this->state.row == grid.dims.height) {
                    this->state.row = 0;

                    for (auto &rule : this->rules.slice()) {
                        rule.onEpochFinish(grid);
                    }

                    if (!this->state.did_epoch_work) {
                        has_next_step = false;
                    }
                }
            }
        }

        return has_next_step;
    }

    auto resetEpoch(Grid grid) -> void {
        // close out any epochs
        for (auto &rule : this->rules.slice()) {
            rule.onEpochFinish(grid);
        }

        for (auto &rule : this->rules.slice()) {
            rule.onEpochStart(grid);
        }
    }

    auto solvable(Grid grid) -> bool {
        while (step(grid))
            ;

        return gridSolved(grid);
    }

    auto getRemainingFlags(Grid grid) -> long {
        assert(grid.mine_count <= static_cast<size_t>(LONG_MAX) &&
               "Mine count exceeds long max");

        long remaining = static_cast<long>(grid.mine_count);
        for (auto &cell : grid.cells) {
            if (cell.display_type == CellDisplayType::cdt_flag) {
                remaining -= 1;
            }
        }

        return remaining;
    }
};
