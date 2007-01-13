########################################################################
# This is a GNU makefile - tested on CYGWIN and Linux
#
# You may need to compile or install libpng and/or zlib before
# compiling this.
#

TARGET = mapgen
# These flags are needed to get it to compile with libpngX.dll on CYGWIN
#CYGWINFLAGS = -DPNG_USE_DLL
CYGWINFLAGS =
PNGFLAGS = -I. 
SDLFLAGS = $(shell sdl-config --cflags)

CC = gcc
CFLAGS += -W -Wall -O0 # -g
LIBS = -L/usr/local/lib -lpng -lm -lSDL_ttf -lSDL_image
LDFLAGS += $(shell sdl-config --libs)

BINS = $(TARGET) $(TARGET).exe
OBJS = mapgen.o alist.o scriptlib.o stringbank.o unicode.o sdlpngdrv.o SDL_gfxPrimitives.o port.o

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $*.c $(PNGFLAGS) $(SDLFLAGS) $(CYGWINFLAGS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS) $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
