all: abx2wav wav2abx

abx2wav: abx2wav.c abx.h abx.c wav.h wav.c port.h
	gcc -W -Wall -g -O0 abx2wav.c abx.c wav.c -o abx2wav

wav2abx: wav2abx.c abx.h abx.c wav.h wav.c port.h
	gcc -W -Wall -g -O0 wav2abx.c abx.c wav.c -o wav2abx

clean:
	rm abx2wav abx2wav.exe wav2abx wav2abx.exe

