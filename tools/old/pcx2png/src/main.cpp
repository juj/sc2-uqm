/*

  Brainless pcx2png converter.

*/

#include "il/ilut.h"

int main(int argc, char** argv)
{
	unsigned int ILid;
	int size;
	char filein[256];
	char filetemp1[256];
	char filetemp2[256];
	char fileout[256];
	
	printf("\n");

	if(argc!=2)
	{
		printf("Usage: pcx2png filename\n");
		exit(0);
	}

	sprintf(filein,"%s",argv[1]);
	sprintf(filetemp1,"%s","deleteme1.pcx");
	sprintf(filetemp2,"%s","deleteme2.pcx");
	sprintf(fileout,"%s",filein);
	size=strlen(fileout);
	fileout[size-3]='p';
	fileout[size-2]='n';
	fileout[size-1]='g';

	ilInit();
	ilGenImages(1,&ILid);
	ilBindImage(ILid);
	ilLoadImage(filein);
	ilSaveImage(filetemp1);
	ilLoadImage(filetemp1);
	ilSaveImage(filetemp2);
	ilLoadImage(filetemp2);
	ilSaveImage(fileout);

	printf("In:  %s\n",filein);
	printf("Out: %s\n",fileout);
}