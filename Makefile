all: build

build:
	gcc wm.c -o main -Wall -Wextra -std=c17 -I/usr/include/freetype2 -lX11 -lXft -lfontconfig -lfreetype
