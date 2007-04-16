unstr: unstr.c
	gcc -W -Wall -g -O0 unstr.c -o unstr

clean:
	rm unstr unstr.tgz

tgz: unstr.c Makefile
	tar -cvzf unstr.tgz unstr.c Makefile

