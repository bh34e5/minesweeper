#pragma once

// auto generated
#include "generated/generated.h"
#include "solver.h"

#include "utils.cc"

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>

static bool initialized = false;
static constexpr size_t plugin_count = ARRAY_LEN(plugin_objs);

static void *plugin_handles[plugin_count];
static RulePlugin *plugins[plugin_count];

auto loadPatternPlugins() -> void {
    assert(!initialized && "Already loaded");

    for (size_t i = 0; i < plugin_count; ++i) {
        void *handle = dlopen(plugin_objs[i], RTLD_NOW);
        if (handle == nullptr) {
            fprintf(stderr, "Failed to open handle (%s): %s\n", plugin_objs[i],
                    dlerror());
            EXIT(1);
        }

        RulePlugin *plugin = (RulePlugin *)dlsym(handle, "plugin");
        if (plugin == nullptr) {
            fprintf(stderr, "Invalid plugin: %s\n", plugin_objs[i]);
            EXIT(1);
        }

        plugin_handles[i] = handle;
        plugins[i] = plugin;
    }
    initialized = true;
}

auto unloadPatternPlugins() -> void {
    assert(initialized && "Unloading uninitialized");

    for (size_t i = 0; i < plugin_count; ++i) {
        dlclose(plugin_handles[i]);
    }
    initialized = false;
}

auto registerPatterns(Arena *arena, GridSolver *solver) -> void {
    assert(initialized && "Registering uninitialized");

    for (size_t i = 0; i < plugin_count; ++i) {
        plugins[i]->regRule(arena, solver);
    }
}

auto deregisterPatterns(GridSolver *solver) -> void {
    assert(initialized && "Deregistering uninitialized");

    for (size_t i = 0; i < plugin_count; ++i) {
        plugins[i]->deregRule(solver);
    }
}
