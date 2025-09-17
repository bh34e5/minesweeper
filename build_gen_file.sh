#!/bin/bash

set -euo pipefail

OUT_DIR=generated
OUT_FILE="$OUT_DIR/generated.h"

UNAME=$(uname -s)

case $(uname -s) in
    Darwin)
        SO=dylib
        ;;
    Linux)
        SO=so
        ;;
    *)
        echo "Unrecognized host"
        return 1
        ;;
esac

rm -f $OUT_FILE

echo "#pragma once" >> $OUT_FILE
echo "" >> $OUT_FILE
echo 'static char const *plugin_objs[] = {' >> $OUT_FILE
for F in patterns/*.pat ; do
    F_ROOT="${F#patterns/}"
    F_ROOT="${F_ROOT%.pat}"

    echo "    "'"'"./pat_${F_ROOT}.${SO}"'"'"," >> $OUT_FILE
done
echo '};' >> $OUT_FILE
