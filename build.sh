#!/bin/bash

set -euo pipefail

function build () {
    echo "Building codegen"

    g++ -g -Wall -Wpedantic -std=c++17 codegen.cc -o codegen

    OUT_DIR=generated
    OUT_FILE="$OUT_DIR/generated.cc"

    mkdir -p $OUT_DIR
    for F in $OUT_DIR/*.cc ; do
        rm -f "$F"
    done

    rm -f $OUT_FILE
    echo "#pragma once" >> $OUT_FILE
    echo "" >> $OUT_FILE
    echo '#include "../solver.cc"' >> $OUT_FILE

    FUNCTIONS=

    for F in patterns/*.pat ; do
        F_ROOT="${F#patterns/}"
        F_ROOT="${F_ROOT%.pat}"

        echo "Generating code for pattern file: $F"

        ./codegen "$F"

        echo "#include "'"'"pat_$F_ROOT.cc"'"' >> $OUT_FILE

        FUNCTIONS="$FUNCTIONS pat_$F_ROOT"
    done

    if ! [[ -z "$FUNCTIONS" ]] ; then
        echo "" >> $OUT_FILE
        echo "auto registerPatterns(GridSolver &solver) -> void {" >> $OUT_FILE
        for FUNC in $FUNCTIONS ; do
            echo "    register_$FUNC(solver);" >> $OUT_FILE
        done
        echo "}" >> $OUT_FILE
    fi

    echo "Building minesweeper"

    LIBS=""

    case "$(uname -s)" in
        Darwin)
            LIBS="-lglfw -framework OpenGL"
            ;;
        Linux)
            LIBS="-lglfw -lGL"
            ;;
        *)
            echo "Unrecognized operating system"
            ;;
    esac

    g++ -g -Wall -Wpedantic -std=c++17 main.cc -o minesweeper $LIBS
}

if [[ $# == 0 ]]; then
    # no arguments, build as normal
    build
elif [[ $# == 1 ]]; then
    # act differently based on the target
    case "$1" in
        "run")
            build
            ./minesweeper
            ;;
        "clean")
            echo "Cleaning"
            [[ -d generated ]] && rm -r generated
            [[ -f codegen ]] && rm codegen
            [[ -f minesweeper ]] && rm minesweeper
            ;;
        *)
            echo "WARNING: Unrecognized argument"
    esac
else
    echo "Unexpected number of arguments, ignoring them"
fi
