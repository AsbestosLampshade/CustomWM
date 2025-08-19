all: build

build:
	gcc wm.c -o main -Wall -Wextra -std=c17 -lX11
