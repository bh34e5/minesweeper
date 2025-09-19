#pragma once

#include "solver.h"

#include "arena.cc"
#include "linkedlist.cc"
#include "strslice.cc"

#include <assert.h>
#include <stdio.h>

auto initSolver(GridSolver *solver, GridApi api) -> void {
    LinkedList<GridSolver::Rule>::initSentinel(&solver->rule_sentinel);
    solver->api = api;
}
