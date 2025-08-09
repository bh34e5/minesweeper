#pragma once

#include <stdlib.h>

#define EXIT myExit

auto myExit(int code) -> void { exit(code); }
