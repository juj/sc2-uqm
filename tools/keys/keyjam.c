#include "SDL.h"

/* Function Prototypes */
void PrintKeyInfo( SDL_KeyboardEvent *key );
void PrintModifiers( SDLMod mod );

    /* main */
int main( int argc, char *argv[] ){
        
    SDL_Event event;
    int quit = 0;
        
    /* Initialise SDL */
    if( SDL_Init( SDL_INIT_VIDEO ) < 0){
	fprintf( stderr, "Could not initialise SDL: %s\n", SDL_GetError() );
	exit( -1 );
    }

    /* Set a video mode */
    if( !SDL_SetVideoMode( 320, 200, 0, 0 ) ){
	fprintf( stderr, "Could not set video mode: %s\n", SDL_GetError() );
	SDL_Quit();
	exit( -1 );
    }

    /* Enable Unicode translation */
    SDL_EnableUNICODE( 1 );

    /* Loop until an SDL_QUIT event is found */
    while( !quit ){

	/* Poll for events */
	while( SDL_PollEvent( &event ) ){
                
	    switch( event.type ){
		/* Keyboard event */
		/* Pass the event data onto PrintKeyInfo() */
	    case SDL_KEYDOWN:
	    case SDL_KEYUP:
		PrintKeyInfo( &event.key );
		break;

		/* SDL_QUIT event (window close) */
	    case SDL_QUIT:
		quit = 1;
		break;

	    default:
		break;
	    }

	}

    }

    /* Clean up */
    SDL_Quit();
    exit( 0 );
}

/* Print all information about a key event */
void PrintKeyInfo( SDL_KeyboardEvent *key ){
    /* Is it a release or a press? */
    if( key->type == SDL_KEYUP )
	printf( "Release:- " );
    else
	printf( "Press:- " );

    /* Print the hardware scancode first */
    printf( "Scancode: 0x%02X", key->keysym.scancode );
    /* Print the name of the key */
    printf( ", Name: %s", SDL_GetKeyName( key->keysym.sym ) );
    /* We want to print the unicode info, but we need to make */
    /* sure its a press event first (remember, release events */
    /* don't have unicode info                                */
    if( key->type == SDL_KEYDOWN ){
	/* If the Unicode value is less than 0x80 then the    */
	/* unicode value can be used to get a printable       */
	/* representation of the key, using (char)unicode.    */
	printf(", Unicode: " );
	if( key->keysym.unicode < 0x80 && key->keysym.unicode > 0 ){
	    printf( "%c (0x%04X)", (char)key->keysym.unicode,
		    key->keysym.unicode );
	}
	else{
	    printf( "? (0x%04X)", key->keysym.unicode );
	}
    }
    printf( "\n" );
    /* Print modifier info */
    PrintModifiers( key->keysym.mod );
}

/* Print modifier info */
void PrintModifiers( SDLMod mod ){
    printf( "Modifers: " );

    /* If there are none then say so and return */
    if( mod == KMOD_NONE ){
	printf( "None\n" );
	return;
    }

    /* Check for the presence of each SDLMod value */
    /* This looks messy, but there really isn't    */
    /* a clearer way.                              */
    if( mod & KMOD_NUM ) printf( "NUMLOCK " );
    if( mod & KMOD_CAPS ) printf( "CAPSLOCK " );
    if( mod & KMOD_LCTRL ) printf( "LCTRL " );
    if( mod & KMOD_RCTRL ) printf( "RCTRL " );
    if( mod & KMOD_RSHIFT ) printf( "RSHIFT " );
    if( mod & KMOD_LSHIFT ) printf( "LSHIFT " );
    if( mod & KMOD_RALT ) printf( "RALT " );
    if( mod & KMOD_LALT ) printf( "LALT " );
    if( mod & KMOD_CTRL ) printf( "CTRL " );
    if( mod & KMOD_SHIFT ) printf( "SHIFT " );
    if( mod & KMOD_ALT ) printf( "ALT " );
    printf( "\n" );
}

