unani: unani.c unani.h
	gcc -W -Wall -g -O0 unani.c -o unani -lpng

clean:
	[ ! -f unani.exe ] || rm unani.exe
	[ ! -f unani ] || rm unani
	rm unani.tgz

tgz: unani.c unani.h Makefile
	tar -cvzf unani.tgz unani.c unani.h Makefile

