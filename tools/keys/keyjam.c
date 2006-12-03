#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "SDL_gfxPrimitives.h"

#define BUFFERSIZE 1024

static int keystate[SDLK_LAST];
static char held_keys[BUFFERSIZE];
static char videoname[BUFFERSIZE];


void
DrawCenteredText (SDL_Surface *s, int y, const char *c)
{
	int w = strlen (c) * 8;
	
	stringRGBA(s, (s->w - w) / 2, y, c, 0xc0, 0xc0, 0xc0, 0xff);
}

void
DrawScreen (SDL_Surface *s)
{	
	int y;
	SDL_FillRect (s, NULL, SDL_MapRGB (s->format, 0, 0, 0x80));
	DrawCenteredText (s, 8, "Key Jamming");

	y = 32;

	DrawCenteredText (s, y, "Many keyboards have hardware that does not permit them to send");
	y += 8;
	DrawCenteredText (s, y, "arbitrary keychords to the computer.  This can complicate games");
	y += 8;
	DrawCenteredText (s, y, "with keyboard control, as activating certain controls may lock out");
	y += 8;
	DrawCenteredText (s, y, "others.  This application lets you pre-test control combinations so");
	y += 8;
	DrawCenteredText (s, y, "as to avoid such conflicts.");

	y += 16;
	DrawCenteredText (s, y, "Test your keyboard by pressing key combinations.  The keyboard's");
	y += 8;
	DrawCenteredText (s, y, "status will be reported in the bottom half of the screen.  When");
	y += 8;
	DrawCenteredText (s, y, "configuring keys for a game, do not select keys that do not all");
	y += 8;
	DrawCenteredText (s, y, "register when simultaneously pressed.");

	y += 16;
	DrawCenteredText (s, y, "You are using the following SDL driver:");
	y += 8;
	DrawCenteredText (s, y, videoname);

	y += 16;
	DrawCenteredText (s, y, "Press ESCAPE to exit.");

	DrawCenteredText (s, 300, held_keys);

	SDL_Flip (s);
}

int 
main (int argc, char *argv[])
{        
	SDL_Event event;
	SDL_Surface *screen;
	int quit = 0;
	int i, changed;
        
	/* Initialise SDL */
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
	{
		fprintf (stderr, "Could not initialise SDL: %s\n", SDL_GetError());
		exit (-1);
	}

	/* Set a video mode */
	if (!(screen = SDL_SetVideoMode (640, 480, 0, SDL_SWSURFACE | SDL_DOUBLEBUF | SDL_ANYFORMAT)))
	{
		fprintf (stderr, "Could not set video mode: %s\n", SDL_GetError());
		SDL_Quit ();
		exit (-1);
	}

	SDL_WM_SetCaption ("Key Jammer", "Key Jammer");

	SDL_VideoDriverName (videoname, BUFFERSIZE);

	for (i = 0; i < SDLK_LAST; i++)
	{
		keystate[i] = 0;
	}

	DrawScreen (screen);

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
			held_keys[0] = '\0';
			for (i = 0; i < SDLK_LAST; i++)
			{
				if (keystate[i])
				{
					if (held_keys[0])
					{
						strcat(held_keys, " | ");
					}
					strcat(held_keys, SDL_GetKeyName (i));
				}
				if (strlen (held_keys) > 200)
				{
					break;
				}
			}
			for (i = 0; i < BUFFERSIZE; i++)
			{
				if (!held_keys[i])
				{
					break;
				}
				held_keys[i] = toupper (held_keys[i]);
			}
			DrawScreen (screen);
		}
		if (keystate[SDLK_ESCAPE])
		{
			quit = 1;
		}
		SDL_Delay (20);
	}

	/* Clean up */
	SDL_Quit ();
	exit (0);
}
