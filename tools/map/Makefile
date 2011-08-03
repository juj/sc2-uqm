########################################################################
# This is a GNU makefile - tested on CYGWIN and Linux
#
# You may need to compile or install libpng and/or zlib before
# compiling this.
# When building on Cygwin, do not use Cygwin-supplied libpng.
#

TARGET = mapgen
# These flags are needed to get it to compile with libpngX.dll on CYGWIN
#CYGWINFLAGS = -DPNG_USE_DLL
PNGFLAGS = -I/usr/include/libpng12
SDLFLAGS = $(shell sdl-config --cflags)

CC = gcc
CFLAGS += -W -Wall -O0 # -g
LIBS = -L/usr/local/lib -lpng12 -lSDL_ttf -lSDL_image
LDFLAGS += $(shell sdl-config --libs)
# This is needed for Cygwin; -L. so that we can link with private libpng
#LDFLAGS += -mconsole -L.

BINS = $(TARGET) $(TARGET).exe
OBJS = mapgen.o alist.o scriptlib.o stringbank.o unicode.o port.o
OBJS += SDL_gfxPrimitives.o
OBJS += sdlpngdrv.o
OBJS += sdlsvgdrv.o

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $*.c $(PNGFLAGS) $(SDLFLAGS) $(CYGWINFLAGS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(BINS) *.o
