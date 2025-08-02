#!/bin/bash

CXX=g++
ENTRY=main.cc
TARGET=minesweeper

$CXX -Wall -Wpedantic -std=c++17 $ENTRY -o $TARGET
