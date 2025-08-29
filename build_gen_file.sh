#!/bin/bash

set -euo pipefail

OUT_DIR=generated
OUT_FILE="$OUT_DIR/generated.cc"

FUNCTIONS=

echo "#pragma once" >> $OUT_FILE
echo "" >> $OUT_FILE
echo '#include "../solver.cc"' >> $OUT_FILE

for F in patterns/*.pat ; do
    F_ROOT="${F#patterns/}"
    F_ROOT="${F_ROOT%.pat}"

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
