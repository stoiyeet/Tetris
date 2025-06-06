#!/bin/sh

# CFLAGS="-std=c++20 -Wall -Wextra -Werror -Wno-c99-designator -fsanitize=undefined,address -ggdb"
CFLAGS="-std=c++20 -I"SDL2/SDL2-2.32.4/include" -I"SDL2_ttf/SDL2_ttf-2.24.0/include" -L"SDL2/SDL2-2.32.4/lib/x64" -L"SDL2_ttf/SDL2_ttf-2.24.0/lib/x64" -Wall -Wextra -Werror -Wno-c99-designator -ggdb -lSDL2main -lSDL2 -lSDL2_ttf -lshell32 -Xlinker /SUBSYSTEM:CONSOLE"
CC="clang++"

#Full Command: clang++ -std=c++20 -I"SDL2/SDL2-2.32.4/include" -I"SDL2_ttf/SDL2_ttf-2.24.0/include" -L"SDL2/SDL2-2.32.4/lib/x64" -L"SDL2_ttf/SDL2_ttf-2.24.0/lib/x64" -Wall -Wextra -Werror -Wno-c99-designator -ggdb -lSDL2main -lSDL2 -lSDL2_ttf -lshell32 -Xlinker /SUBSYSTEM:CONSOLE tetris.cpp -o tetris.exe

set -xe

$CC $CFLAGS tetris.cpp -o tetris.exe
