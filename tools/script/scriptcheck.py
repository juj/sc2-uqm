#!/usr/bin/python

import string
import os.path

class Speech:
    def __init__(self, id=None, audio=None):
        self.id = id
        self.audio = audio
        self.lines = []
    def add_line (self, line):
        if len(line) != 0 and line[-1] == '\n': line = line[:-1]
        self.lines.append(line)
    def clean (self):
        while self.lines != []:
            if self.lines[-1].strip() != "": break
            self.lines.pop()
    def check (self):
        # Check that non-first lines don't start with space
        weird = 0
        ever_weird = 0
        for line in self.lines[1:]:
            if len(line) != 0 and line[0] in string.whitespace:
                if line.strip() != "": weird = 1
        if weird:
            print "Warning: "+self.id+" has whitespace preceding an internal line"
            ever_weird = 1
        # Now check for non-last lines ending with space
        weird = 0
        for line in self.lines[:-1]:
            if len(line) != 0 and line[-1] in string.whitespace:
                if line.strip() != "":
                    if line.strip()[-1] not in string.punctuation: weird = 1
        if weird:
            print "Warning: "+self.id+" has whitespace trailing an internal line"
            ever_weird = 1
        if ever_weird:
            print_boring_style(self)

def print_old_style(speech):
    header = "#("+speech.id+")"
    if speech.audio is not None:
        header += "\t"+speech.audio
    print header
    for line in speech.lines:
        if line == "":
            print " "
        else:
            print line
    print ""

def print_boring_style(speech):
    print 'ID is "'+speech.id+'"'
    if speech.audio is not None:
        print 'Audio is "'+speech.audio+'"'
    for line in speech.lines:
        print '\t"'+line+'"'
    print ""

def read_old_file(filename):
    current = None
    speeches = []
    for line in file(filename):
        if line[0] == '#':
            lparen = line.index('(')
            rparen = line.rindex(')')
            if (lparen > rparen): raise ValueError
            id = line[(lparen+1):rparen]
            if (lparen != 1):
                print "Warning: Speech "+id+" in "+filename+" isn't flush left"
            audio = line[rparen+1:].strip()
            if audio == "": audio = None
            current = Speech(id, audio)
            speeches.append(current)
        else:
            current.add_line(line)
    for speech in speeches:
        speech.clean()
    return speeches

def read_source_header(filename):
    ids = []
    active = 0
    for line in file(filename):
        if active == 0 and line.find('{') != -1:
            active = 1
        elif active == 1 and line.find('}') != -1:
            active = 2
        elif active == 1:
            try:
                id = line[:line.rindex(',')].strip()
                if id[0] != "/": ids.append (id)
            except ValueError:
                id = line.strip()
                if id != "": ids.append (id)
    return ids

races = ['arilou', 'blackur', 'chmmr', 'comandr', 'druuge',
         'ilwrath', 'melnorm', 'mycon', 'orz', 'pkunk', 'rebel',
         'shofixt', 'slyhome', 'slyland', 'spahome', 'spathi',
         'starbas', 'supox', 'syreen', 'talkpet', 'thradd',
         'umgah', 'urquan', 'utwig', 'vux', 'yehat', 'zoqfot']

root = os.path.join('..','..','sc2')

for race in races:
    try:
        filename = os.path.join(root,'content','comm',race,race+'.txt')
        source = os.path.join(root,'src','sc2code','comm',race,'strings.h')
        print "Checking: "+race
        data = read_old_file(filename)
        id_seq = read_source_header(source)
        for speech in data:
            speech.check()
        if id_seq[0] != "NULL_PHRASE":
            print "Error!  First element of enum not NULL_PHRASE!"
        id_seq = id_seq[1:]
        for x in map (None, data, id_seq):
            if x[0] == None:
                print "Error!  enum "+x[1]+" has no partner!"
            elif x[1] == None:
                print "Warning!  Data entry "+x[0].id+" is never referenced!"
            elif x[0].id != x[1]:
                print "Warning!  text identifier "+x[0].id+" doesn't match enum constant "+x[1]+"!"
    except ValueError:
        print "Malformed header line"
    except IOError:
        print "I/O Error"
