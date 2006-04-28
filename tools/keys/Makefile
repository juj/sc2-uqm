all: keyjam.c
	gcc $(shell sdl-config --cflags) -o keyjam keyjam.c $(shell sdl-config --libs) -lSDL_gfx

clean:
	rm -f keyjam
