# this is a GNU Makefile.  It is not expected to work with anything other
# than gmake
FILES  = audio.cpp audioSDL.cpp synch.cpp synchronizer.cpp
TARGET = synch

# For MacOS X: SDL, Ogg and Vorbis are provided as frameworks,
# which require different flags than unix-style shared libs
ifeq ($(shell uname), Darwin)
    CFLAGS  += -D__MACOSX__
    LDFLAGS += -framework Ogg -framework Vorbis -framework SDL \
              -framework Cocoa
else
    CFLAGS  += $(shell sdl-config --cflags)
    LDFLAGS += -lvorbisfile -lvorbis $(shell sdl-config --libs)
endif

CFLAGS  += -DUSE_SDLAUDIO=1 $(shell fltk-config --cxxflags)
LDFLAGS += $(shell fltk-config --ldflags)

CFLAGS += -W -Wall \
                -Wwrite-strings -Wimplicit -Wreturn-type -Wformat \
                -Wswitch -Wcomment -Wchar-subscripts \
                -Wparentheses -Wcast-align -Waggregate-return \
                -Winline
#		-Wcast-qual

# MacOS X apps need to have a resource fork applied, else the window
# manager will refuse to manage them (which makes the app very
# difficult to use)  This rule should be ignored by all non-Mac
# platforms, and it assumes that the Apple Developer Tools and FLTK
# are installed in their default locations.  Edit to taste if
# appropriate.
ifeq ($(shell uname), Darwin)
REZ   = /Developer/Tools/Rez
RFORK = /usr/local/include/FL/mac.r
$(TARGET)/rsrc: $(TARGET)
	$(REZ) -t APPL $(RFORK) -o $(TARGET)
endif
# END silly MacOS X goo

$(TARGET): $(FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(FILES) -o $@

clean:
	$(RM) $(TARGET)
