#!/bin/sh

# CFLAGS="-std=c++20 -Wall -Wextra -Werror -Wno-c99-designator -fsanitize=undefined,address -ggdb"
CFLAGS="-std=c++20 -ISDL2/SDL2-2.32.4/include -LSDL2/SDL2-2.32.4/lib/x64 -Wall -Wextra -Werror -Wno-c99-designator -ggdb -lSDL2"
CC="clang++"

#Full Command: clang++ -std=c++20 -I"SDL2/SDL2-2.32.4/include" -L"SDL2/SDL2-2.32.4/lib/x64" tetris.cpp -o tetris -std=c++20 -Wall -Wextra -Werror -Wno-c99-designator -ggdb -lSDL2

set -xe

$CC $CFLAGS tetris.cpp -o tetris -lm
