#include <windows.h>
#include <wingdi.h>
#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <SDL/SDL.h>

void process_input(void);
void init_sdl(void);
SDL_Surface *init_video(void);
void showLastError(char *c);

static char* font_names[] = { "SC2 Arilou", "SC2 Chmmr", "SC2 Earthling",
	"SC2 Ilwrath", "SC2 Kohr-Ah", "SC2 Melnorme/Druuge", "SC2 Mycon",
	"SC2 Orz", "SC2 Pkunk", "SC2 Shofixti", "SC2 Slylandro",
	"SC2 Slylandro Probe", "SC2 Spathi", "SC2 Supox", "SC2 Syreen",
	"SC2 Talking Pet", "SC2 Thraddash", "SC2 Umgah", "SC2 Ur-Quan",
	"SC2 Utwig", "SC2 VUX", "SC2 Yehat", "SC2 ZoqFotPik" };
#define NUM_FONTS 23
int currentFont = 0;

void init(void) {
	HDC hdc;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_FLAT);
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
}

void display(void) {
	HDC hdc;
	HGDIOBJ hgdiobj;

	glClear(GL_COLOR_BUFFER_BIT);

	if ((hdc = wglGetCurrentDC()) == NULL) {
		showLastError("wglGetCurrentDC failed");
	}
	if ((hgdiobj = SelectObject(hdc, CreateFont(0, 0, 0, 0, FW_DONTCARE,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		font_names[currentFont]))) == NULL) {
		showLastError("SelectObject failed");
	}
	if (!wglUseFontBitmaps(hdc, 0, 255, 1000)) {
		showLastError("wglUseFontBitmaps failed");
	}

	glListBase(1000);
	glRasterPos2d(-.9, .15);
	glCallLists(strlen(font_names[currentFont]), GL_UNSIGNED_BYTE,
		font_names[currentFont]);
	glRasterPos2d(-.9, 0);
	glCallLists(28, GL_UNSIGNED_BYTE, "Press UP/DOWN to change font");
	glRasterPos2d(-.9, -.15);
	glCallLists(23, GL_UNSIGNED_BYTE, "0123 ABC abc !@#$%^&*()");

	if (DeleteObject(hgdiobj) == 0) {
		showLastError("DeleteObject failed");
	}
	glFlush();
	SDL_GL_SwapBuffers();
}

int main(int argc, char*argv[]) {
	SDL_Surface *screen;

	init_sdl();
	init_video();
	init();
	while (1) {
		display();
		process_input();
	}
	return 0;
}

// The following functions are mostly unimportant -- they just set up opengl and sdl

// Shows last Windows error in a message box using GetLastError()
void showLastError(char *c) {
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);

	// Display the string.
	MessageBox( NULL, lpMsgBuf, c, MB_OK|MB_ICONINFORMATION );

	// Free the buffer.
	LocalFree( lpMsgBuf );
}

//the following functions merely deal with OpenGL and SDL

void process_input(void) {
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_KEYDOWN:
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					exit(0);
				}
				if (e.key.keysym.sym == SDLK_UP) {
					currentFont --;
					if (currentFont < 0) {
						currentFont = NUM_FONTS - 1;
					}
				}
				if (e.key.keysym.sym == SDLK_DOWN) {
					currentFont ++;
					if (currentFont == NUM_FONTS) {
						currentFont = 0;
					}
				}

				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
	}
}

void init_sdl(void) {
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);

    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
}

SDL_Surface *init_video(void) {
	SDL_Surface *screen;

    screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE | SDL_OPENGL);
    if ( screen == NULL ) {
        fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
        exit(1);
    }

	return screen;
}
