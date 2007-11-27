#!/usr/bin/python
import sys
import os
import os.path

def get_lists(d):
    result = []
    for (dirpath, dirnames, filenames) in os.walk(d):
        for l in [f for f in filenames if f.endswith('.lst')]:
            result.append(os.path.join(dirpath, l))
    return result

def read_list(f):
    result = []
    for line in file(f):
        parsed = line.split()
        if len(parsed) == 4:
            result.append(((int(parsed[0]), int(parsed[1]), int(parsed[2])),
                          parsed[3]))
    return result

if __name__ == "__main__":
    print "This is a library and isn't intended to be run directly."
