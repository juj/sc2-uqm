#!/usr/bin/python

import sys
import os.path
import Dialogue

races = ['arilou', 'blackur', 'chmmr', 'comandr', 'druuge',
         'ilwrath', 'melnorm', 'mycon', 'orz', 'pkunk', 'rebel',
         'shofixt', 'slyhome', 'slyland', 'spahome', 'spathi',
         'starbas', 'supox', 'syreen', 'talkpet', 'thradd', 'utwig',
         'umgah', 'urquan', 'vux', 'yehat', 'zoqfot']

root = os.path.join('..','..','sc2')

race_speeches = {}

def load_txt():
    print "Loading .txt files...",
    for race in races:
        try:
            filename = os.path.join(root,'content','comm',race,race+'.txt')
            speeches = Dialogue.Speeches(file(filename))
            race_speeches[race] = speeches
            print race,
        except ValueError:
            print "["+race+" didn't parse]",
        except IOError:
            print "["+race+" didn't load]",
        sys.stdout.flush()
    print ""

def do_commands():
    def do_delete(speeches, argstr):
        args = argstr.split()
        # Check all args before deleting any of them
        for arg in args:
            temp = speeches.speechdict[arg]
        for arg in args:
            speeches.deletespeech(arg)
    def do_fuse(speeches, argstr):
        i = argstr.index('=')
        left = argstr[:i].split()
        right = argstr[i+1:].split()
        if len(left) != 1: raise ValueError
        if len(right) == 0: raise ValueError
        new_id = left[0]
        old_id = right[0]
        speeches.renamespeech(old_id, new_id)
        # Split out tags from items.
        is_tag = 1
        tags = []
        speechIDs = []
        for arg in right[1:]:
            if is_tag:
                tags.append(arg)
                is_tag = 0
            else:
                speechIDs.append(arg)
                is_tag = 1
        # Now do the actual join
        for tag, speech in map(None, tags, speechIDs):
            speeches.joinspeech(new_id, tag, speech)
        for speech in speechIDs:
            speeches.deletespeech(speech)
    def do_add(speeches, argstr):
        args = argstr.split()
        if len(args) == 0: raise ValueError
        for arg in args[1:]:
            speeches.joinspeech(args[0], None, arg)
    def do_addstr(speeches, argstr):
        args = argstr.split()
        if len(args) == 0: raise ValueError
        id = args[0]
        str = " ".join(args[1:])
        speeches.addtospeech(id, str)
    def do_pretag(speeches, argstr):
        args = argstr.split()
        if len(args) != 2: raise ValueError
        id = args[0]
        tag = args[1]
        speeches.pretag(id, tag)
    directives = {'delete':do_delete, 'fuse':do_fuse,
                  'add':do_add, 'addstr':do_addstr,
                  'pretag':do_pretag}
    print "Processing command file..."
    try:
        cmdfile = file(sys.argv[1])
        for cmd in cmdfile:
            try:
                i = cmd.index(':')
                left = cmd[:i].split()
                right = cmd[i+1:]
                if len(left) != 2: raise ValueError
                directive = directives[left[0].lower()]
                race = race_speeches[left[1]]
                directive(race, right)
            except ValueError:
                if cmd.strip() != "":
                    print "Malformed command '%s'" % cmd.strip()
            except KeyError:
                print "Bad ID in '%s'" % cmd.strip()
        cmdfile.close()
    except IndexError:
        print "Well, we would have if it were specified."
    except IOError:
        print "Well, we would have had the file specified actually existed."
    
def write_XML():
    print "Writing XML...",
    for race in races:
        try:
            speeches = race_speeches[race]
            outfilename = os.path.join('.', race+'.xml');
            race_xml = speeches.as_xml()
            xmlfile = file(outfilename, 'wt')
            xmlfile.write(race_xml)
            xmlfile.close()
            print race,
        except KeyError:
            print "["+race+" didn't load or process]",
        except IOError:
            print "["+race+" couldn't write]",
        sys.stdout.flush()
    print ""
        
if __name__ == "__main__":
    load_txt()
    do_commands()
    write_XML()
