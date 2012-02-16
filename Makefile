all: src/lxbread.c src/map_lib.c
	clang src/lxbread.c src/map_lib.c -Wall -O2 -o lxbread
