#!/bin/sh

CFLAGS="-std=c++20 -Wall -Wextra -Werror -Wno-c99-designator -fsanitize=undefined,address -ggdb"
CC="clang++"

set -xe

$CC $CFLAGS tetris.cpp -o tetris -lm
