#include <stdio.h>
#include <stdlib.h>
#include "SDL.h"

static int keystate[SDLK_LAST];

int 
main (int argc, char *argv[])
{        
	SDL_Event event;
	int quit = 0;
	int i, changed;
        
	/* Initialise SDL */
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
	{
		fprintf (stderr, "Could not initialise SDL: %s\n", SDL_GetError());
		exit (-1);
	}

	/* Set a video mode */
	if (!SDL_SetVideoMode (320, 200, 0, 0))
	{
		fprintf (stderr, "Could not set video mode: %s\n", SDL_GetError());
		SDL_Quit ();
		exit (-1);
	}

	for (i = 0; i < SDLK_LAST; i++)
	{
		keystate[i] = 0;
	}

	/* Loop until an SDL_QUIT event is found */
	while( !quit ) {
		changed = 0;
		/* Poll for events */
		while (SDL_PollEvent (&event))
		{
			switch( event.type )
			{
			case SDL_KEYDOWN:
				keystate[event.key.keysym.sym] = 1;
				changed = 1;
				break;
			case SDL_KEYUP:
				keystate[event.key.keysym.sym] = 0;
				changed = 1;
				break;
			case SDL_QUIT:
				quit = 1;
				break;

			default:
				break;
			}
		}
		if (changed) {
			printf ("Status:");
			for (i = 0; i < SDLK_LAST; i++)
			{
				if (keystate[i])
				{
					printf(" %s", SDL_GetKeyName (i));
				}
			}
			printf ("\n");
		}
	}

	/* Clean up */
	SDL_Quit ();
	exit (0);
}
