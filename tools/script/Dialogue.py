#!/usr/bin/python
import sys
import string

def _xml_line(line, firstline):
    if len(line) == 0 and firstline: return ''
    return '      <line> '+line.strip()+' </line>\n'

def _xml_join(a, tag, b):
    if tag is None:
        return a.strip() + ' '+ b.lstrip()
    result=a.strip()
    if a != '' and a[-1] in string.whitespace:
        result += " <space/>"
    result += ' <special type="'+tag+'"/> '
    if b != '' and b[0] in string.whitespace:
        result += "<space/> "
    result +=  b.lstrip()
    return result

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
    def join (self, tag, other):
        if self.lines == []:
            self.lines = ['']
        if other is None:
            self.lines[-1] = _xml_join(self.lines[-1], tag, '')
            return
        if other.lines == []:
            other.lines = ['']
        self.lines[-1] = _xml_join(self.lines[-1], tag, other.lines[0])
        self.lines.extend(other.lines[1:])        

class Speeches:
    def __init__(self, datafile):
        current = None
        speeches = []
        self.speechdict = {}
        for line in datafile:
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
        for speech in speeches: speech.clean()
        self.speecharray = speeches
        self.__rehash()
    def as_xml(self):
        result =  '<?xml version="1.0" encoding="UTF-8"?>\n'
        result += '<!DOCTYPE speech SYSTEM "speech.dtd">\n\n'
        result += '<speeches>\n'
       
        for speech in self.speecharray:
            result += '  <speech id="'+speech.id+'">\n'
            if speech.audio is not None:
                result += '    <audio><file>'+speech.audio+'</file></audio>\n'
            result += '    <text>\n'
            for line, i in zip(speech.lines, xrange(sys.maxint)):
                result += _xml_line(line, i==0)
            result += '    </text>\n'
            result += '  </speech>\n'

        result += '</speeches>\n'
        return result
    def __rehash(self):
        self.speechdict = {}
        for speech, i in zip(self.speecharray, xrange(sys.maxint)):
            self.speechdict[speech.id]=i
    def deletespeech(self, id):
        i = self.speechdict[id]
        del self.speecharray[i]
        self.__rehash()
    def renamespeech(self, oldid, newid):
        i = self.speechdict[oldid]
        del self.speechdict[oldid]
        self.speechdict[newid]=i
        self.speecharray[i].id=newid
    def joinspeech(self, srcid, tag, nextid):
        src = self.speecharray[self.speechdict[srcid]]
        if nextid is not None:
            next = self.speecharray[self.speechdict[nextid]]
        else:
            next = None
        src.join(tag, next)
    def addtospeech(self, id, str):
        src = self.speecharray[self.speechdict[id]]
        if src.lines == []:
            src.lines = [str]
        else:
            src.lines[-1] = _xml_join(src.lines[-1], None, str)
    def pretag(self, id, tag):
        src = self.speecharray[self.speechdict[id]]
        if src.lines == []:
            src.lines = ['']
        src.lines[0] = _xml_join('', tag, src.lines[0])


if __name__ == '__main__':
    print "This is the core library for the Ur-Quan Masters dialogue translators."
    print "You don't need to run it directly."
