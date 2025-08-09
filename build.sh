#!/bin/bash

set -exuo pipefail

g++ -g -Wall -Wpedantic -std=c++17 codegen.cc -o codegen

for f in patterns/*.pat ; do
    ./codegen "$f"
done

g++ -g -Wall -Wpedantic -std=c++17 main.cc -o minesweeper
