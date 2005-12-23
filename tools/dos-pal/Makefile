########################################################################
# This is a GNU makefile - tested on CYGWIN and Linux
#
# You may need to compile or install libpng and/or zlib before
# compiling this.
#

TARGET1 = pal2uqm
TARGET2 = palarith

# These flags are needed to get it to compile with libpngX.dll on CYGWIN
#CYGWINFLAGS =

CC = gcc
CFLAGS = -W -Wall -O0 # -g
LIBS = -L/usr/local/lib -lm
LDFLAGS=

BINS = $(TARGET1) $(TARGET2) $(TARGET1).exe $(TARGET2).exe
SRCS = pal2uqm.c palarith.c
OBJS1 = pal2uqm.o
OBJS2 = palarith.o

.SUFFIXES: .c .o

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $*.c $(CYGWINFLAGS)

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1) $(LIBS) $(LDFLAGS)

$(TARGET2): $(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2) $(LIBS) $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
