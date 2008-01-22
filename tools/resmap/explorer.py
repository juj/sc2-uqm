#!/usr/bin/python
import sys
import os
import os.path
import re

def collect_extension(d, ext):
    result = []
    for (dirpath, dirnames, filenames) in os.walk(d):
        for l in [f for f in filenames if f.endswith(ext)]:
            result.append(os.path.join(dirpath, l))
    return result

def collect_res_headers(d, pattern="i.*\\.h$"):
	result = []
	regex = re.compile(pattern) 
	for (dirpath, dirnames, filenames) in os.walk(d):
		for l in [f for f in filenames if regex.match(f) != None]:
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
    
def read_header(f):
	result = []
	for line in file(f):
		line = line.strip()
		if line.startswith("#define"):
			parsed = [x.strip() for x in line.split()]
			if len(parsed) == 3 and parsed[2].startswith('0') and parsed[2].endswith('L'):
				result.append((parsed[1], parsed[2]))
	return result

if __name__ == "__main__":
    print "This is a library and isn't intended to be run directly."
