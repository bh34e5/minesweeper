#pragma once

#include "dlist.cc"
#include "grid.cc"

#include <stdio.h>
#include <sys/types.h>

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

    auto registerRule(Rule rule) -> void { this->rules.push(rule); }

    auto solvable(Grid grid, bool verbose = false) -> bool {
        bool did_work = true;

        if (verbose) {
            printGrid(grid);
            printf("\n");
        }

        size_t epoch_count = 0;
        while (did_work) {
            did_work = false;

            if (verbose) {
                printf("Epoch %zu\n", ++epoch_count);
            }

            for (auto &rule : this->rules.slice()) {
                rule.onEpochStart(grid);
            }

            for (size_t r = 0; r < grid.dims.height; ++r) {
                for (size_t c = 0; c < grid.dims.width; ++c) {
                    for (auto &rule : this->rules.slice()) {
                        bool rule_did_work = rule.applyRule(grid, r, c);
                        did_work = did_work || rule_did_work;

                        if (verbose && rule_did_work) {
                            printGrid(grid);
                            printf("\n");
                        }
                    }
                }
            }

            for (auto &rule : this->rules.slice()) {
                rule.onEpochFinish(grid);
            }
        }

        return gridSolved(grid);
    }
};
