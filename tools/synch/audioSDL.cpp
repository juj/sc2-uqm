//
//
// Ur-Quan Masters Voice-over / Subtitle Synchronization tool
// Copyright 2003 by Geoffrey Hausheer
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// contact tcpaiqgvnmbf28f001@sneakemail.com for bugs/issues
//
#if (USE_SDLAUDIO == 1)
char AUDIO_TYPE[] = "SDL";

#ifdef __APPLE__
#  include <SDL/SDL.h>
#else
#  include <SDL.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "audio.h"

/* doesn't Win32 have M_PI? */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
	SDL_AudioSpec spec;
	char *data;
	unsigned long data_len;
	unsigned long data_pos;
} WAVESTRUCT;


void fill_audio(void *u, Uint8 *stream, int len);

unsigned long clocks_per_sec = 1000;

unsigned long
get_clock()
{
	return (SDL_GetTicks());
}

void
init_sound ()
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr,"could not initialize SDL audio\n");
		exit(-1);
	}
}

WAVESTRUCT *
play_sound(AUDIOHDR *audiohdr)
{
	WAVESTRUCT *WaveData = new WAVESTRUCT;
/*
	WaveData->spec.freq = 22050;
	WaveData->spec.format = AUDIO_S16;
	WaveData->spec.channels = 2;
	WaveData->spec.samples = 1024;
	WaveData->spec.callback = fill_audio;
*/
	WaveData->spec.freq = audiohdr->samp_freq;
	WaveData->spec.format = AUDIO_S16;
	WaveData->spec.channels = audiohdr->channels;
	WaveData->spec.samples = 1024;
	WaveData->spec.callback = fill_audio;
	WaveData->spec.userdata = WaveData;
	WaveData->data = audiohdr->data;
	WaveData->data_len = audiohdr->data_len;
	WaveData->data_pos = 0;
	if(SDL_OpenAudio(&WaveData->spec, NULL) < 0) {
		fprintf(stderr,"could not open SDL audio\n");
		exit(-1);
	}
	SDL_PauseAudio(0);
	return (WaveData);
}

void
pause_sound(WAVESTRUCT *WaveData)
{
	if (WaveData)
		SDL_PauseAudio(1);
}

void
unpause_sound (WAVESTRUCT *WaveData)
{
	if (WaveData)
		SDL_PauseAudio(0);
}

void
reset_sound (WAVESTRUCT *WaveData)
{
	if (WaveData) {
		SDL_CloseAudio();
		delete WaveData->data;
		delete WaveData;
	}
}

/*
void fill_audio (void *u, Uint8 *stream, int len)
{
        Sint16 *b;
        int l, i;
        double factor;

        printf("%p is %d\n", u, *(int*)u);
        factor = Frequency / 22050.0 * 2.0 * M_PI;
        l = len * sizeof *stream / sizeof *b; // yuck
        b = (Sint16 *)malloc(l * sizeof *b);
        for(i = 0; i < l / 2; i++, Offset++) {
                double v = cos(Offset * factor);
                b[i * 2] = b[i * 2 + 1] = (int)(v * 32767);
        }
        memcpy(stream, b, len);
        free(b);
}
*/

void
fill_audio (void *u, Uint8 *stream, int len)
{
	long amt_left;
	WAVESTRUCT *WaveData = (WAVESTRUCT *)u;

	amt_left = WaveData->data_len - WaveData->data_pos;
	if (amt_left > len)
		amt_left = len;
	memcpy(stream, WaveData->data + WaveData->data_pos, amt_left);
	WaveData->data_pos += amt_left;
}

#endif

