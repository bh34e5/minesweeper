#pragma once

#include "solver.h"

#include "arena.cc"
#include "linkedlist.cc"
#include "strslice.cc"

#include <assert.h>
#include <stdio.h>

auto initSolver(GridSolver *solver) -> void {
    LinkedList<GridSolver::Rule>::initSentinel(&solver->rule_sentinel);
}

auto makeRule(GridSolver::Rule::Apply *apply_fn, StrSlice name)
    -> GridSolver::Rule {
    GridSolver::Rule rule{apply_fn, nullptr, nullptr, nullptr, name};
    return rule;
}

auto makeRule(GridSolver::Rule::Apply *apply_fn,
              GridSolver::Rule::OnEpochStart *on_epoch_start_fn,
              GridSolver::Rule::OnEpochFinish *on_epoch_finish_fn, void *data,
              StrSlice name) -> GridSolver::Rule {
    GridSolver::Rule rule{apply_fn, on_epoch_start_fn, on_epoch_finish_fn, data,
                          name};
    return rule;
}
