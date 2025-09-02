#pragma once

#include "../arena.cc"
#include "../solver.cc"
#include "pat_12.cc"
#include "pat_13_1.cc"
#include "pat_13_2.cc"
#include "pat_14.cc"

auto registerPatterns(Arena *arena, GridSolver *solver) -> void {
    register_pat_12(arena, solver);
    register_pat_13_1(arena, solver);
    register_pat_13_2(arena, solver);
    register_pat_14(arena, solver);
}
