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
#if (USE_SDLAUDIO != 1)
char AUDIO_TYPE[] = "Native";

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "audio.h"

typedef struct {
	HWAVEOUT hWaveOut;
	WAVEFORMATEX wfx;
	WAVEHDR header;
	int paused;
	unsigned long time;

} WAVESTRUCT;

unsigned long clocks_per_sec = CLOCKS_PER_SEC;
unsigned long get_clock()
{
	return (clock());
}

void init_sound ()
{
}

void reset_sound (WAVESTRUCT *WaveData)
{
	if (WaveData)
	{
		waveOutReset (WaveData->hWaveOut);
		waveOutUnprepareHeader(WaveData->hWaveOut, &WaveData->header, sizeof(WAVEHDR));
	    waveOutClose(WaveData->hWaveOut);
		delete WaveData->header.lpData;
		delete WaveData;
	}
}

void pause_sound (WAVESTRUCT *WaveData)
{
	if (WaveData)
		waveOutPause (WaveData->hWaveOut);
}

void unpause_sound (WAVESTRUCT *WaveData)
{
	if (WaveData)
		waveOutRestart (WaveData->hWaveOut);
}

WAVESTRUCT *play_sound(AUDIOHDR *audiohdr)


    {
	WAVESTRUCT *WaveData = new WAVESTRUCT;
	WaveData->hWaveOut = 0;
	WaveData->wfx.nSamplesPerSec = audiohdr->samp_freq;
	WaveData->wfx.wBitsPerSample = audiohdr->bits_per_sample;
	WaveData->wfx.nChannels = audiohdr->channels;
	WaveData->wfx.cbSize = 0;
	WaveData->wfx.wFormatTag = WAVE_FORMAT_PCM;
	WaveData->wfx.nBlockAlign = (WaveData->wfx.wBitsPerSample >> 3) * WaveData->wfx.nChannels;
	WaveData->wfx.nAvgBytesPerSec = WaveData->wfx.nBlockAlign * WaveData->wfx.nSamplesPerSec;

    if(waveOutOpen(&WaveData->hWaveOut, WAVE_MAPPER, &WaveData->wfx,
		0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        fprintf(stderr, "unable to open WAVE_MAPPER device\n");
        return(0);
    }
    ZeroMemory(&WaveData->header, sizeof(WAVEHDR));
    WaveData->header.dwBufferLength = audiohdr->data_len;
    WaveData->header.lpData = audiohdr->data;
    waveOutPrepareHeader(WaveData->hWaveOut, &WaveData->header, sizeof(WAVEHDR));
    waveOutWrite(WaveData->hWaveOut, &WaveData->header, sizeof(WAVEHDR));
	return (WaveData);
}
#endif
