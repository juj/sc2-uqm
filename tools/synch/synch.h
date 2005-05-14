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
#include "audio.h"

#define MAX_LEN 200
#define MAX_TRACKS 300
typedef struct CLIPTXTDATA { 
	char str[MAX_LEN];
	unsigned long time;
	int frame_num;
	CLIPTXTDATA *Next;
	CLIPTXTDATA ()
	{
		str[0] = 0;
		time = 0;
		frame_num = 0;
		Next = NULL;
	}
	~CLIPTXTDATA ()
	{
		if(Next !=NULL)
			delete Next;
	}
} CLIPTXTDATA;

typedef struct FILEDATA {
	char filename[PATH_MAX];
	int num_lines;
	long chksum;
	unsigned long tracktime;
	CLIPTXTDATA *clip;
	FILEDATA ()
        {
		num_lines = 0;
		clip = NULL;
		chksum = 0;
		tracktime = 0;
		filename[0] = 0;
	}
	~FILEDATA()
	{
		if (clip != NULL)
		delete clip;
		clip = NULL;
	}
} FILEDATA;

typedef struct XTRATRACKDATA {
	unsigned long time;
	int tracknum;
	int paused;
	XTRATRACKDATA () {
		time = 0;
		tracknum = 0;
		paused = 0;
	}
} XTRATRACKDATA;

int read_ogg (const char *filename,  AUDIOHDR *audiohdr);
unsigned long get_ogg_runtime (const char *filename);
unsigned long checksum(const char *filename);


unsigned long get_clock();
extern unsigned long clocks_per_sec;
extern char AUDIO_TYPE[];

typedef struct {int bogus;} WAVESTRUCT;
void init_sound ();
void reset_sound (WAVESTRUCT *WaveData);
void pause_sound (WAVESTRUCT *WaveData);
void unpause_sound (WAVESTRUCT *WaveData);
WAVESTRUCT *play_sound(AUDIOHDR *audiohdr);

