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
#define SYNC_VER "0.12"
#define SKIP_SINGLE_LINES
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef WIN32
#	include <io.h>
#	include <direct.h>
#	define MKDIR mkdir
#	define PATH_MAX _MAX_PATH
#else
#	include <unistd.h>
#	define MKDIR(dir) mkdir((dir), 0777)
#	include <limits.h>
#endif

#ifdef __linux__
  #define SYNC_EXTRA_VER "Linux"
#elif defined WIN32
  #define SYNC_EXTRA_VER "Windows"
#else
  #define SYNC_EXTRA_VER "Unknown"
#endif
#include <math.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include "synchronizer.h"

#include "synch.h"

#define WRAP_LEN 50

const char races[][13] = {
	"arilou",
	"blackur",
	"chmmr",
	"comandr",
	"druuge",
	"ilwrath",
	"melnorm",
	"mycon",
	"orz",
	"pkunk",
	"rebel",
	"shofixt",
	"slyhome",
	"slyland",
	"spahome",
	"spathi",
	"starbas",
	"supox",
	"syreen",
	"talkpet",
	"thradd",
	"umgah",
	"urquan",
	"utwig",
	"vux",
	"yehat",
	"zoqfot",
	""
};

FILEDATA *curtrack, *racedata[MAX_TRACKS];
CLIPTXTDATA *playing_clip;
XTRATRACKDATA *trackxtra;
Synchronizer *synch;
WAVESTRUCT *WaveData;

char path[80];
char savepath[80] = ".";


int
main(int argc, char **argv) {
	int i = 0;
	synch = new Synchronizer;
	trackxtra = new XTRATRACKDATA;
        init_sound ();
//Initial global objects.
	while (strlen(races[i]))
	{
		synch->RaceChooser->add (races[i], (void *)i);
		i++;
	}

	synch->show(argc, argv);

	return Fl::run();
}

void CleanUp()
{
	int i;
	int max = synch->TrackSelector->nitems();

	for (i=0;i<max;i++)
		if (racedata[i])
			delete racedata[i];
}
void DoExit()
{
	reset_sound (WaveData);
	CleanUp();
	exit(0);
}
void DeactivateOptions()
{
	synch->TrackSelector->deactivate();
	synch->RaceChooser->deactivate();
	synch->ChooseAllButton->deactivate();
	synch->ClearAllButton->deactivate();
}
void ActivateOptions()
{
	synch->TrackSelector->activate();
	synch->RaceChooser->activate();
	synch->ChooseAllButton->activate();
	synch->ClearAllButton->activate();
}

void PrintFrameNum ()
{
	char buf[20];
	sprintf(buf,"%d of %d",playing_clip->frame_num, curtrack->num_lines);
	synch->CurrentFrame->value(buf);
}

void UpdatePlayTime (int start)
{
	int i, max, mins, secs;
	unsigned long tracktime = 0;
	char buf[20];
	max = synch->TrackSelector->nitems();
	for (i= start; i <= max; i++) 
		if (synch->TrackSelector->checked(i))
			tracktime += racedata[i-1]->tracktime;
	mins = tracktime / 60;
	secs = tracktime - 60 * mins;
	sprintf (buf,"%d:%02d",mins,secs);
	synch->PlayTime->value(buf);
}

void StopPlayback ()
{
	reset_sound (WaveData);
	WaveData=NULL;
	trackxtra->tracknum = 0;
	ActivateOptions();
}

void StartPlayback ()
{
	int i, start, max;
	max = synch->TrackSelector->nitems();
	if (! trackxtra->tracknum)
	{
		start = 1;
	} else {
		start = trackxtra->tracknum + 1;
	}
	trackxtra->tracknum = 0;
	for (i=start; i<= max; i++) {
		if (synch->TrackSelector->checked(i))
		{
			trackxtra->tracknum = i;
			break;
		}
	}
	if (trackxtra->tracknum)
	{
		char buf[80];
		AUDIOHDR audiohdr;
		trackxtra->paused = 0;
		reset_sound (WaveData);
		WaveData = NULL;
		DeactivateOptions();
		curtrack = racedata[trackxtra->tracknum-1];
		playing_clip = curtrack->clip;
		if (! playing_clip) {
			fprintf (stderr, "Couldn't find track text for %s\n",curtrack->filename);
			return;
		}
		synch->CurrentTrack->value(curtrack->filename);
		UpdatePlayTime(trackxtra->tracknum);
		PrintFrameNum();
		synch->TextScreen->value(playing_clip->str);
		sprintf (buf, "%s/%s",path, curtrack->filename);
		if (read_ogg (buf,  &audiohdr)) {
			fprintf (stderr, "Ogg Read failed!\n");
			playing_clip = 0;
			return;
		}
		curtrack->chksum = checksum(buf);
		WaveData = play_sound(&audiohdr);
		trackxtra->time=get_clock();
	} else {
		StopPlayback();
	}
}


void RestartFrame ()
{
	if (trackxtra->tracknum) {
		trackxtra->tracknum--;
	}
	StartPlayback();
}

void SaveTimestamps()
{
	char buf[80], buf1[80];
	CLIPTXTDATA *curclip;
	FILE *fh;
	strcpy (buf1, curtrack->filename);
	buf1[strlen(buf1)-4]=0;
	sprintf (buf, "%s/%s.ts",savepath, buf1);
	fh = fopen(buf,"w");
	fprintf(fh,"Version: %s-%s-%s\n",SYNC_VER, SYNC_EXTRA_VER, AUDIO_TYPE);
	fprintf(fh,"Checksum: %lu\n", curtrack->chksum);
	curclip = curtrack->clip;
	while (curclip != NULL) {
		fprintf(fh,"%8.3f\n",(double)(curclip->time)/clocks_per_sec);
		curclip = curclip->Next;
	}
	fclose (fh);
}


void NextFrame ()
{
	unsigned long curtime;
	if (!trackxtra->tracknum || trackxtra->paused)
		return;
	if (playing_clip->Next == NULL)
		StartPlayback();
	else {
		curtime = get_clock();
		printf ("Frame time: %lu\n",curtime-trackxtra->time);
		playing_clip = playing_clip->Next;
		playing_clip->time = curtime-trackxtra->time;
		synch->TextScreen->value(playing_clip->str);
		PrintFrameNum();
		if (playing_clip->Next == NULL)
			SaveTimestamps();
	}
		
}

void
wrapstr (char *dst, const char *src)
{
	size_t i,j,last=0;
	size_t src_len = strlen(src);
	for (i = 0; i < src_len; i++) {
		*dst++ = src[i];
		if (i == last) {
			if (i != 0)
				*dst++ = '\n';

			last = src_len;
			for (j=i+1; j < src_len; j++) {
				if (j -i > WRAP_LEN)
					break;
				if (src[j] == ' ')
					last = j;
			}
			if (j == src_len)
				last = j;
		}
	}
	*dst = 0;
}

void
PauseFrame ()
{
	unsigned long curtime;
	if(trackxtra->paused)
		unpause_sound(WaveData);
	else
		pause_sound(WaveData);
	curtime = get_clock();
	trackxtra->time = curtime - trackxtra->time;
	trackxtra->paused = ! trackxtra->paused;
}

void
GetTracks (int value)
{
	const char *race;
	char speechFileName[PATH_MAX];
	char str[MAX_LEN];
	int i = 0, read_data = 0;
	CLIPTXTDATA *curclip;
	FILE *fh;

	race = races[value - 1];
	CleanUp();
	sprintf (path, "comm/%s", race);
	sprintf (speechFileName, "%s/%s.txt", path, race);
	fh = fopen (speechFileName, "r");
	if (!fh) {
		fl_alert ("Could not find %s!", speechFileName);
		return;
	}
	MKDIR ("timestamp");
	sprintf(savepath,"timestamp/%s",race);
	MKDIR (savepath);
	synch->TrackSelector->clear ();
	while (fgets(str, 512, fh))
	{
		str[strlen (str) - 1] = 0;
		if(str[0] == '#')
		{
			char *tmp;
			read_data = 0;
			tmp = strtok (str, "\t ");
			tmp = strtok (NULL, "\t ");
			if (tmp != NULL)
			{
#ifdef SKIP_SINGLE_LINES
				if (i && racedata[i-1]->num_lines < 2) {
					delete racedata[i-1];
					i--;
				}
#endif
				if (i && ! racedata[i-1]->tracktime) {
					char buf[PATH_MAX];
					sprintf(buf,"%s/%s", path, racedata[i-1]->filename);
					racedata[i-1]->tracktime = get_ogg_runtime(buf);
					synch->TrackSelector->add (racedata[i-1]->filename);
					if (! racedata[i-1]->tracktime) {
						fprintf(stderr, "couldn't read %s\n", racedata[i-1]->filename);
						racedata[i-1]->tracktime = 1;
					}
				}
				i++;
				racedata[i-1] = new FILEDATA;
				strncpy (racedata[i-1]->filename, tmp, PATH_MAX);
				racedata[i-1]->filename[PATH_MAX - 1] = '\0';
				curclip = NULL;
				read_data = 1;
			}
		}
		else if (read_data && str[0] != '\0')
		{
			if (racedata[i-1]->clip == NULL) {
				racedata[i-1]->clip = new CLIPTXTDATA;
				curclip = racedata[i-1]->clip;
			} else {
				curclip->Next = new CLIPTXTDATA;
				curclip = curclip->Next;
			}
			wrapstr (curclip->str, str);
			racedata[i-1]->num_lines++;
			curclip->frame_num = racedata[i-1]->num_lines;
		}
		if (feof (fh))
			break;

	}
#ifdef SKIP_SINGLE_LINES
	if (i && racedata[i-1]->num_lines < 2) {
		delete racedata[i-1];
		i--;
	}
#endif
	if (i && ! racedata[i-1]->tracktime) {
		char buf[PATH_MAX];
		sprintf(buf,"%s/%s",path, racedata[i-1]->filename);
		racedata[i-1]->tracktime = get_ogg_runtime(buf);
		synch->TrackSelector->add (racedata[i-1]->filename);
	}
	synch->TrackSelector->position (1);
}

unsigned long
get_ogg_runtime (const char *filename) {
	OggVorbis_File vf;
	FILE *fh;
	unsigned long runtime;
	double rt;
	fh = fopen(filename, "rb");
	if (fh == NULL)
		return (0);
	if (ov_open(fh, &vf, NULL, 0) < 0)
		return (0);
	rt = ov_time_total(&vf, -1);
	runtime = (unsigned long)(rt + 0.5);
	ov_clear(&vf);
	fclose(fh);
	printf("%s : %lu (%f)\n", filename, runtime, rt);
	return(runtime);
}

int
read_ogg (const char *filename,  AUDIOHDR *audiohdr) {
	OggVorbis_File vf;
	vorbis_info *vi;
	FILE  *fh;
	int current_section;

	fh = fopen(filename, "rb");
	if (fh == NULL) {
		fprintf (stderr, "Couldn't find file: %s\n", filename);
		return (1);
	}
	if (ov_open(fh, &vf, NULL, 0) < 0) {
		fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
		return (1);
	}
	vi = ov_info(&vf,-1);
	audiohdr->samp_freq = vi->rate;
	audiohdr->channels = vi->channels;
	audiohdr->bits_per_sample = 16;
	{
		const unsigned int extra = 10000;
		unsigned int numsamples = extra + (unsigned int)ov_pcm_total(&vf,0);
		unsigned int numbytes = numsamples * vi->channels * 2;
				// 16bit output wav
		char *wavptr;
		int eof = 0;
		unsigned long readbytes = 0;
		audiohdr->data = new char[numbytes];
		wavptr = audiohdr->data;
		while(!eof){
			long ret = ov_read(&vf, wavptr, numbytes - readbytes,
					0, 2, 1, &current_section);
			if (ret == 0) {
				eof = 1;
			} else if (ret > 0) {
				readbytes += ret;
				wavptr += ret;
			}
			// XXX - What if ret < 0 - SvdB
		}
		audiohdr->data_len = readbytes;
	}
	ov_clear(&vf);
	fclose(fh);
	return (0);
}

unsigned long
checksum(const char *filename)
{
	int fh;
	unsigned long chk = 0, buf = 0;
	fh = open(filename,O_RDONLY);
	if (fh == -1)
		return 0;
	while (read(fh, &buf, sizeof(buf)) > 0)
			// XXX Not endianness-safe - SvdB
		chk = ((chk << 1) | (chk >> 31)) ^ buf;
	close(fh);
	return (chk);
}


