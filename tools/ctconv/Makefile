########################################################################
# This is a GNU makefile - tested on CYGWIN and Linux
#
# You may need to compile or install libpng and/or zlib before
# compiling this.
#

TARGET = ctconv
# These flags are needed to get it to compile with libpngX.dll on CYGWIN
# CYGWINFLAGS = -DPNG_USE_DLL
CYGWINFLAGS =

CC = gcc
CFLAGS = -W -Wall -O0 # -g
LIBS = -L/usr/local/lib -lm
LDFLAGS=

BINS = $(TARGET) $(TARGET).exe
SRCS = ctconv.c
OBJS = ctconv.o

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $*.c $(CYGWINFLAGS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS) $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
