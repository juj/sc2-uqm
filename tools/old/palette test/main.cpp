/******************************************************************************

  Palette Shifting Test
  Copyright (C) 2002  Stuart Binge  (datura@mighty.co.za)

  This  program is free software;  you can  redistribute it and / or modify it
  under the terms of the GNU  General Public License as  published by the Free
  Software  Foundation;  either version 2 of the License,  or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,  but WITHOUT
  ANY  WARRANTY;  without  even  the implied  warranty of  MERCHANTABILITY  or
  FITNESS  FOR A PARTICULAR PURPOSE.  See the GNU  General Public  License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 Temple
  Place - Suite 330, Boston, MA  02111-1307, USA.  You can also  view the  GNU
  General Public License, online, at the GNU Homepage: http://www.gnu.org

 ******************************************************************************/

/*
 * NOTE: If you are building with MSVC, make sure that the "Use runtime library"
 * setting is "Multithreaded DLL", in "Project->Settings->C++->Code Generation".
 * Use this setting for both debug and runtime builds. Also use "Win32
 * Application" as the project template, NOT "Win32 Console Application", when
 * creating a project for this.
 */

/*
 * NOTE: You'll need to add the OpenGL, GLU, and SDL libs to your project if your
 * compiler does not understand these lib pragma's.
 */
#pragma comment(lib, "opengl32") 
#pragma comment(lib, "glu32")
#pragma comment(lib, "sdl")
#pragma comment(lib, "sdlmain")

#include <C:/SDL-1.2.4/include/SDL.h>

#ifdef _WIN32
// Win32 GL headers require that windows.h be included before them
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#include <iostream>
#include <fstream>

#include <cstdio>
#include <cmath>
#include <climits>

const double pi = 3.14159265358979323846;

bool g_bRunning = true;
bool g_bRotate = false;
bool g_bCycle1 = false;
bool g_bCycle2 = false;

const int g_iWidth = 640;
const int g_iHeight = 480;
const bool g_bFullscreen = false;

GLuint g_uiTexName = 0;
char* g_pcData;
int g_iTexWid;
int g_iTexHei;
GLfloat g_rpal[256];
GLfloat g_gpal[256];
GLfloat g_bpal[256];
GLfloat g_apal[256];
SDL_Surface* g_ssAppWindow;

/*
 * NOTE: The following structures are assumed to be in little-endian format
 * (i.e. as used on Intel-32 architecture).
 */

/*
 * NOTE: if not building with MSVC, or if your compiler does not understand
 * pack pragma's, then change this next structure to somehow use 1-byte
 * alignment.
 */
#pragma pack(push, 1)

struct sRGBTriple {
	Uint8 rgbt_Blue;
	Uint8 rgbt_Green;
	Uint8 rgbt_Red;
};

#pragma pack(pop)

struct sRGBQuad {
	Uint8 rgbq_Blue;
	Uint8 rgbq_Green;
	Uint8 rgbq_Red;
	Uint8 rgbq_Reserved;
};	 

/*
 * NOTE: if not building with MSVC, or if your compiler does not understand
 * pack pragma's, then change this next structure to somehow use 2-byte
 * alignment.
 */
#pragma pack(push, 2)

struct sBitmapFileHeader {
	Uint16 bfh_Type;
	Uint32 bfh_Size;
	Uint16 bfh_Reserved1;
	Uint16 bfh_Reserved2;
	Uint32 bfh_OffBits;
};

#pragma pack(pop)

// Currently defined valid bitmap headers

// This first header uses sRGBTriple's for its colour map
struct sBitmapHeader_1 {
	Uint16 bh1_Width;
	Uint16 bh1_Height;
	Uint16 bh1_Planes;
	Uint16 bh1_BitCount;
};

// Headers 2, 3 and 4 have this structure in common
struct sStandardBH {
	Uint32 Width;
	Uint32 Height;
	Uint16 Planes;
	Uint16 BitCount;
	Uint32 Compression;
	Uint32 SizeImage;
	Uint32 XPelsPerMeter;
	Uint32 YPelsPerMeter;
	Uint32 ClrUsed;
};

// The next three headers use sRGBQuad's for their colour map
struct sBitmapHeader_2 {
	sStandardBH std;
	Uint32 _unused[1];
};

struct sBitmapHeader_3 {
	sStandardBH std;
	Uint32 _unused[18];
};

struct sBitmapHeader_4 {
	sStandardBH std;
	Uint32	_unused[22];
};

// End of structures

bool img_Load8bppBMP(std::string filename) {
	if (filename.empty()) return false;

	std::ifstream file(filename.c_str(), std::ios_base::binary | std::ios_base::in);
	if (!file) return false;

	sBitmapFileHeader bfh;

	file.read(reinterpret_cast<char*>(&bfh), sizeof(sBitmapFileHeader));
	if (file.bad() || file.eof()) return false;

	if (bfh.bfh_Type != (('M' << 8) | 'B')) return false;

	Uint32 hdrsz;

	file.read(reinterpret_cast<char*>(&hdrsz), sizeof(Uint32));
	if (file.bad() || file.eof()) return false;

	hdrsz -= sizeof(Uint32);

	bool trip = false;

	if (hdrsz == sizeof(sBitmapHeader_1)) {
		sBitmapHeader_1 bh;
		file.read(reinterpret_cast<char*>(&bh), sizeof(sBitmapHeader_1));
		if (file.bad() || file.eof() || bh.bh1_BitCount != 8) return false;

		g_iTexWid = bh.bh1_Width;
		g_iTexHei = bh.bh1_Height;
		trip = true;
	} else if (hdrsz == sizeof(sBitmapHeader_2)) {
		sBitmapHeader_2 bh;
		file.read(reinterpret_cast<char*>(&bh), sizeof(sBitmapHeader_2));
		if (file.bad() || file.eof() || bh.std.BitCount != 8) return false;

		g_iTexWid = bh.std.Width;
		g_iTexHei = bh.std.Height;
	} else if (hdrsz == sizeof(sBitmapHeader_3)) {
		sBitmapHeader_3 bh;
		file.read(reinterpret_cast<char*>(&bh), sizeof(sBitmapHeader_3));
		if (file.bad() || file.eof() || bh.std.BitCount != 8) return false;

		g_iTexWid = bh.std.Width;
		g_iTexHei = bh.std.Height;
	} else if (hdrsz == sizeof(sBitmapHeader_4)) {
		sBitmapHeader_4 bh;
		file.read(reinterpret_cast<char*>(&bh), sizeof(sBitmapHeader_4));
		if (file.bad() || file.eof() || bh.std.BitCount != 8) return false;

		g_iTexWid = bh.std.Width;
		g_iTexHei = bh.std.Height;
	}

	bool invert = true;
	if (g_iTexHei < 0) {
		invert = false;
		g_iTexHei = -g_iTexHei;
	}

	if (trip) {
		sRGBTriple tmppal[256];
		file.read(reinterpret_cast<char*>(tmppal), sizeof(sRGBTriple) * 256);
		
		for (int i = 0; i < 256; i++) {
			g_rpal[i] = (float)tmppal[i].rgbt_Red / 255.0f;
			g_gpal[i] = (float)tmppal[i].rgbt_Green / 255.0f;
			g_bpal[i] = (float)tmppal[i].rgbt_Blue / 255.0f;
		}
	} else {
		sRGBQuad tmppal[256];
		file.read(reinterpret_cast<char*>(tmppal), sizeof(sRGBQuad) * 256);
		
		for (int i = 0; i < 256; i++) {
			g_rpal[i] = (float)tmppal[i].rgbq_Red / 255.0f;
			g_gpal[i] = (float)tmppal[i].rgbq_Green / 255.0f;
			g_bpal[i] = (float)tmppal[i].rgbq_Blue / 255.0f;
		}
	}

	g_pcData = new char[g_iTexWid * g_iTexHei];
	file.read(g_pcData, g_iTexWid * g_iTexHei);

	if (invert) {
		char* tmpbuf = new char[g_iTexWid];
		for (int i = 0; i < g_iTexHei / 2; i++) {
			memcpy(tmpbuf, g_pcData + (g_iTexHei - 1 - i) * g_iTexWid, g_iTexWid);
			memcpy(g_pcData + (g_iTexHei - 1 - i) * g_iTexWid, g_pcData + i * g_iTexWid, g_iTexWid);
			memcpy(g_pcData + i * g_iTexWid, tmpbuf, g_iTexWid);
		}
		delete[] tmpbuf;
	}

	return true;
}

static void inp_KeyHandler(SDL_keysym* keysym) {
	switch (keysym->sym) {
		case SDLK_q:
			g_bCycle1 = !g_bCycle1;
			break;

		case SDLK_w:
			g_bCycle2 = !g_bCycle2;
			break;

		case SDLK_ESCAPE:
			g_bRunning = false;
			break;

/*		case SDLK_RETURN:
			if ((keysym->mod | KMOD_ALT) > 0)
				SDL_WM_ToggleFullScreen(g_ssAppWindow);
			break;*/

		case SDLK_SPACE:
			g_bRotate = !g_bRotate;
			break;
	}
}

void app_ProcessEvents() {
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
				inp_KeyHandler(&event.key.keysym);
				break;

			case SDL_QUIT:
				g_bRunning = false;
				break;
		}

	}

}

void app_DrawFrame() {
	static float rot = 0.0f;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(0.0f, 0.0f, -3.0f);

	if (g_bRotate) rot += 1.0f;

	glRotatef(rot, 0.0, 1.0, 0.0);

	if (g_bCycle1) {
		GLfloat tmp[4];
		tmp[0] = g_rpal[256 - 16 - 1];
		tmp[1] = g_gpal[256 - 16 - 1];
		tmp[2] = g_bpal[256 - 16 - 1];
		tmp[3] = g_apal[256 - 16 - 1];
		for (int i = 256 - 16 - 1; i > 0; i--) {
			g_rpal[i] = g_rpal[i-1];
			g_gpal[i] = g_gpal[i-1];
			g_bpal[i] = g_bpal[i-1];
			g_apal[i] = g_apal[i-1];
		}

		g_rpal[0] = tmp[0];
		g_gpal[0] = tmp[1];
		g_bpal[0] = tmp[2];
		g_apal[0] = tmp[3];
	}

	if (g_bCycle2) {
		GLfloat tmp[4];
		tmp[0] = g_rpal[256 - 1];
		tmp[1] = g_gpal[256 - 1];
		tmp[2] = g_bpal[256 - 1];
		tmp[3] = g_apal[256 - 1];
		for (int i = 256 - 1; i > 256 - 16; i--) {
			g_rpal[i] = g_rpal[i-1];
			g_gpal[i] = g_gpal[i-1];
			g_bpal[i] = g_bpal[i-1];
			g_apal[i] = g_apal[i-1];
		}

		g_rpal[256 - 16] = tmp[0];
		g_gpal[256 - 16] = tmp[1];
		g_bpal[256 - 16] = tmp[2];
		g_apal[256 - 16] = tmp[3];
	}

	if (g_bCycle1 || g_bCycle2) {
		glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 256, g_rpal);
		glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 256, g_gpal);
		glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 256, g_bpal);
		glPixelMapfv(GL_PIXEL_MAP_I_TO_A, 256, g_apal);

		glBindTexture(GL_TEXTURE_2D, g_uiTexName);
		glTexSubImage2D
		(
			GL_TEXTURE_2D,
			0, 0, 0,
			g_iTexWid, g_iTexHei,
			GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
			g_pcData
		);
	}

	glBegin(GL_QUADS);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture(GL_TEXTURE_2D, g_uiTexName);

		// Front face
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, -1.0f, 0.0f);

		// Back face
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(-1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(1.0f, -1.0f, 0.0f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(-1.0f, -1.0f, 0.0f);
	glEnd();

	glFlush();
}

void ogl_Setup() {
	glViewport(0, 0, g_iWidth, g_iHeight);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity(); 
	
	gluPerspective(45.0, static_cast<float>(g_iWidth) / static_cast<float>(g_iHeight), 0.1, 100);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glClearDepth(1.0f);
	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST); 

	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);

	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);	

	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &g_uiTexName);

	img_Load8bppBMP(std::string("test.bmp"));

	for (int i = 0; i < 256; i++) g_apal[i] = 1.0f;

/*	std::ofstream tf("maps.txt");
	for (i = 0; i < 256; i++) {
		tf << i << " R: " << g_rpal[i] << ", G: " << g_gpal[i] << ", B: " << g_bpal[i] << ", A: " << g_apal[i] << std::endl;			
	}	  */

	/*std::ofstream bin("index.bin", std::ios_base::binary | std::ios_base::out);
	bin.write(g_data, 512*512);*/

	glPixelTransferi(GL_MAP_COLOR, GL_TRUE);

	glPixelMapfv(GL_PIXEL_MAP_I_TO_R, 256, g_rpal);
	glPixelMapfv(GL_PIXEL_MAP_I_TO_G, 256, g_gpal);
	glPixelMapfv(GL_PIXEL_MAP_I_TO_B, 256, g_bpal);
	glPixelMapfv(GL_PIXEL_MAP_I_TO_A, 256, g_apal);

	glBindTexture(GL_TEXTURE_2D, g_uiTexName);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, g_iTexWid, g_iTexHei, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, g_pcData);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

int main(int argc, char** argv){
/*	std::ofstream f("test.txt");

	f << "sizeof(sRGBTriple) = " << sizeof(sRGBTriple) << ", sizeof(RGBTRIPLE) = " << sizeof(RGBTRIPLE) << std::endl;
	f << "sizeof(sRGBQuad) = " << sizeof(sRGBQuad) << ", sizeof(RGBQUAD) = " << sizeof(RGBQUAD) << std::endl;
	f << "sizeof(sBitmapFileHeader) = " << sizeof(sBitmapFileHeader) << ", sizeof(BITMAPFILEHEADER) = " << sizeof(BITMAPFILEHEADER) << std::endl;
	f << "sizeof(sBitmapHeader_1) = " << sizeof(sBitmapHeader_1) << ", sizeof(BITMAPCOREHEADER) = " << sizeof(BITMAPCOREHEADER) << std::endl;
	f << "sizeof(sBitmapHeader_2) = " << sizeof(sBitmapHeader_2) << ", sizeof(BITMAPINFOHEADER) = " << sizeof(BITMAPINFOHEADER) << std::endl;
	f << "sizeof(sBitmapHeader_3) = " << sizeof(sBitmapHeader_3) << ", sizeof(BITMAPV4HEADER) = " << sizeof(BITMAPV4HEADER) << std::endl;
	f << "sizeof(sBitmapHeader_4) = " << sizeof(sBitmapHeader_4) << ", sizeof(BITMAPV5HEADER) = " << sizeof(BITMAPV5HEADER) << std::endl;*/

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "Video initialisation failed: " << SDL_GetError() << std::endl;
		return 1;
	}

	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	if (!info) {
		std::cerr << "Video information query failed: " << SDL_GetError() << std::endl;
		return 1;
	}

	int bpp = info->vfmt->BitsPerPixel;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	int flags = SDL_OPENGL | (g_bFullscreen ? SDL_FULLSCREEN : 0);

	if ((g_ssAppWindow = SDL_SetVideoMode(g_iWidth, g_iHeight, bpp, flags)) == 0) {
		std::cerr << "Video mode set failed: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_WM_SetCaption("Palette Shifting Test", 0);

	ogl_Setup();

	for (;;) {
		app_ProcessEvents();
		if (!g_bRunning) break;
		app_DrawFrame();
		SDL_GL_SwapBuffers();
	}

	delete[] g_pcData;

	glDeleteTextures(1, &g_uiTexName);

	return 0;
}
