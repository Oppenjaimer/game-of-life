CFLAGS=-std=c99 -Wall -Wextra
LIBS=.\SDL2\lib -lmingw32 -lSDL2main -lSDL2
INCLUDES=.\SDL2\include\SDL2

all:
	gcc main.c -o game-of-life $(CFLAGS) -L$(LIBS) -I$(INCLUDES)