unpkg: unpkg.c unpkg.h
	gcc -W -Wall -g -O0 unpkg.c -o unpkg

parseres: parseres.c parseres.h
	gcc -W -Wall -g -O0 parseres.c -o parseres

clean:
	rm unpkg parseres unpkg.tgz

tgz: unpkg.c unpkg.h Makefile parseres.c parseres.h
	tar -cvzf unpkg.tgz unpkg.c unpkg.h Makefile parseres.c parseres.h

