DEPS := $(wildcard *.d)

FLAGS := -g -MMD -Wall -Wpedantic -std=c++17

LIBS_Darwin := -lglfw -framework OpenGL
LIBS_Linux  := -lglfw -lGL

LIBS := ${LIBS_${shell uname -s}}

PATTERNS  := $(wildcard patterns/*.pat)
GENERATED := $(patsubst patterns/%.pat,generated/pat_%.cc,$(PATTERNS))

.PHONY: all clean run

all: minesweeper codegen

run: minesweeper
	./minesweeper

minesweeper: main.cc generated/generated.cc
	@echo Building minesweeper
	g++ $(FLAGS) $< -o $@ $(LIBS)

codegen: codegen.cc
	@echo Building codegen
	g++ $(FLAGS) $< -o $@ $(LIBS)

generated/generated.cc: $(GENERATED) build_gen_file.sh
	$(shell if [ -f generated/generated.cc ] ; then rm generated/generated.cc ; fi)
	./build_gen_file.sh

generated/pat_%.cc: patterns/%.pat codegen | generated
	./codegen $<

generated:
	mkdir $@

clean:
	rm -rf generated
	rm -f minesweeper
	rm -f codegen
	rm -f *.d

-include $(DEPS)
