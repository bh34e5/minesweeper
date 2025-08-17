#pragma once

#include "../solver.cc"
#include "pat_12.cc"
#include "pat_13_1.cc"
#include "pat_13_2.cc"
#include "pat_14.cc"

auto registerPatterns(GridSolver &solver) -> void {
    register_pat_12(solver);
    register_pat_13_1(solver);
    register_pat_13_2(solver);
    register_pat_14(solver);
}
