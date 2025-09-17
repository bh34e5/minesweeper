#pragma once

#include "grid.h"

#include "arena.cc"
#include "linkedlist.cc"
#include "strslice.cc"
#include "version.cc"

#include <assert.h>
#include <stddef.h>

struct SolveState {
    size_t row;
    size_t col;
    size_t rule;
    bool did_epoch_work;
    size_t last_work_rule;
    bool invalid;
};

struct GridSolver {
    struct Rule {
        typedef auto(Apply)(Grid *grid, size_t row, size_t col, void *data)
            -> bool;
        typedef auto(OnEpochStart)(Grid *grid, void *data) -> void;
        typedef auto(OnEpochFinish)(Grid *grid, void *data) -> void;

        Apply *apply;
        OnEpochStart *onStart;
        OnEpochFinish *onFinish;
        void *data;
        StrSlice name;

        auto applyRule(Grid *grid, size_t row, size_t col) -> bool {
            if (this->apply == nullptr) {
                return false;
            }
            return this->apply(grid, row, col, this->data);
        }

        auto onEpochStart(Grid *grid) -> void {
            if (this->onStart == nullptr) {
                return;
            }
            this->onStart(grid, this->data);
        }

        auto onEpochFinish(Grid *grid) -> void {
            if (this->onFinish == nullptr) {
                return;
            }
            this->onFinish(grid, this->data);
        }
    };

    LinkedList<Rule> rule_sentinel;
    size_t rule_count;
    SolveState state;

    auto registerRule(Arena *arena, Rule rule) -> void {
        // TODO(bhester): assert no duplicate rules

        LinkedList<Rule> *ll = arena->pushT<LinkedList<Rule>>({rule});

        this->rule_sentinel.enqueue(ll);
        ++this->rule_count;

        this->state.invalid = true;
    }

    auto deregisterRule(StrSlice name) -> Rule {
        LinkedList<Rule> *ll = &this->rule_sentinel;
        while ((ll = ll->next) != &this->rule_sentinel) {
            if (ll->val.name.eql(name)) {
                ll->next->prev = ll->prev;
                ll->prev->next = ll->next;

                // FIXME(bhester): free list of rules?

                --this->rule_count;

                return ll->val;
            }
        }

        assert(0 && "Rule not found");
    }

    auto step(Grid *grid) -> bool {
        bool did_work = false;
        bool should_continue = true;
        while (!did_work && should_continue) {
            should_continue = this->stepInner(grid, &did_work);
        }
        return did_work;
    }

    auto stepInner(Grid *grid, bool *did_work) -> bool {
        if (this->state.invalid) {
            this->reset(grid);
        }

        if (this->state.row == 0 && this->state.col == 0 &&
            this->state.rule == 0) {
            LinkedList<Rule> *ll = &this->rule_sentinel;
            while ((ll = ll->next) != &this->rule_sentinel) {
                ll->val.onEpochStart(grid);
            }

            this->state.did_epoch_work = false;
        }

        size_t rule_to_apply = this->state.rule++;
        Rule &rule = this->ruleAt(rule_to_apply);
        *did_work = rule.applyRule(grid, this->state.row, this->state.col);

        if (*did_work) {
            this->state.did_epoch_work = true;
            this->state.last_work_rule = rule_to_apply;
        }

        bool has_next_step = true;
        if (this->state.rule == this->rule_count) {
            this->state.rule = 0;
            ++this->state.col;

            if (this->state.col == grid->dims.width) {
                this->state.col = 0;
                ++this->state.row;

                if (this->state.row == grid->dims.height) {
                    this->state.row = 0;

                    LinkedList<Rule> *ll = &this->rule_sentinel;
                    while ((ll = ll->next) != &this->rule_sentinel) {
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

    auto ruleAt(size_t idx) -> Rule & {
        assert(idx < this->rule_count);

        size_t i = 0;
        LinkedList<Rule> *ll = &this->rule_sentinel;
        while ((ll = ll->next) != &this->rule_sentinel && i < idx) {
            ++i;
        }

        return ll->val;
    }

    auto reset(Grid *grid) -> void {
        this->resetEpoch(grid);

        this->state.row = 0;
        this->state.col = 0;
        this->state.rule = 0;
        this->state.did_epoch_work = false;
        this->state.last_work_rule = 0;
        this->state.invalid = false;
    }

    auto resetEpoch(Grid *grid) -> void {
        // close out any epochs
        LinkedList<Rule> *ll = &this->rule_sentinel;
        while ((ll = ll->next) != &this->rule_sentinel) {
            ll->val.onEpochFinish(grid);
        }
    }

    auto solvable(Grid *grid) -> bool {
        this->reset(grid);
        while (this->step(grid))
            ;

        return gridSolved(*grid);
    }
};

#define REGISTERER(Name, arena, solver)                                        \
    void(Name)(Arena * arena, GridSolver * solver)
#define DEREGISTERER(Name, solver) void(Name)(GridSolver * solver)

typedef REGISTERER(RuleRegisterer, arena, solver);
typedef DEREGISTERER(RuleDeregisterer, solver);

struct RulePlugin {
    RuleRegisterer *regRule;
    RuleDeregisterer *deregRule;
};

auto initSolver(GridSolver *solver) -> void;
auto makeRule(GridSolver::Rule::Apply *apply_fn, StrSlice name)
    -> GridSolver::Rule;
auto makeRule(GridSolver::Rule::Apply *apply_fn,
              GridSolver::Rule::OnEpochStart *on_epoch_start_fn,
              GridSolver::Rule::OnEpochFinish *on_epoch_finish_fn, void *data,
              StrSlice name) -> GridSolver::Rule;
