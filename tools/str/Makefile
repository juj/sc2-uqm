unstr: unstr.c unstr.h
	gcc -W -Wall -g -O0 unstr.c -o unstr

clean:
	rm unstr unstr.tgz

tgz: unstr.c unstr.h Makefile
	tar -cvzf unstr.tgz unstr.c unstr.h Makefile

