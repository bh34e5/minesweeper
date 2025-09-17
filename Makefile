DEPS := $(wildcard *.d)

FLAGS := -g -MMD -Wall -Wpedantic -std=c++17

LIBS_Darwin := -lglfw -framework OpenGL
LIBS_Linux  := -lglfw -lGL

DBG_Darwin := lldb
DBG_Linux  := gdb

SO_Darwin := dylib
SO_Linux := so

UNAME := $(shell uname -s)
LIBS  := ${LIBS_$(UNAME)}
DBG   := ${DBG_$(UNAME)}
SO    := ${SO_$(UNAME)}

PATTERNS  := $(wildcard patterns/*.pat)
GENERATED := $(patsubst patterns/%.pat,generated/pat_%.cc,$(PATTERNS))
PLUGINS   := $(patsubst patterns/%.pat,pat_%.$(SO),$(PATTERNS))

.PHONY: all clean gen-files run debug plugins

all: minesweeper codegen plugins

run: minesweeper
	./minesweeper

debug: minesweeper
	$(DBG) ./minesweeper

minesweeper: main.cc | generated/generated.h
	@echo Building minesweeper
	g++ $(FLAGS) $< -o $@ $(LIBS)

codegen: codegen.cc
	@echo Building codegen
	g++ $(FLAGS) $< -o $@ $(LIBS)

gen-files: generated/generated.h

plugins: one_of_aware.$(SO) $(PLUGINS)

generated/generated.h: $(GENERATED) build_gen_file.sh
	./build_gen_file.sh

generated/pat_%.cc: patterns/%.pat codegen | generated
	./codegen $<

generated:
	mkdir $@

one_of_aware.$(SO): one_of_aware.cc
	g++ $(FLAGS) -shared -fPIC $< -o $@ -Wl,-undefined,dynamic_lookup

pat_%.$(SO): generated/pat_%.cc
	g++ $(FLAGS) -shared -fPIC $< -o $@ -Wl,-undefined,dynamic_lookup

clean:
	rm -rf generated
	rm -f minesweeper
	rm -f codegen
	rm -f *.d
	rm -f *.$(SO)
	rm -rf *.dSYM/

-include $(DEPS)
