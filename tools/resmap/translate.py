#!/usr/bin/env python

import sys
from gen_resfiles import *

class FileRemap:
	def __init__(self, src, dest):
		self.src = src
		self.dest = dest
	def as_script(self):
		return "%s -> %s" % (self.src, self.dest)
	def svn_shell(self):
		return ["svn move %s %s" % (self.src, self.dest)]
	def transform(self, x):
		if x.res_file == self.src:
			x.res_file = self.dest

class DirRemap:
	def __init__(self, src, dest):
		self.src = src
		self.dest = dest
	def as_script(self):
		return "%s => %s" % (self.src, self.dest)
	def svn_shell(self):
		return ["svn move %s %s" % (self.src, self.dest)]
	def transform(self, x):
		if x.res_file.startswith(self.src):
			x.res_file = self.dest + x.res_file[len(self.src):]

def load_script(fname):
	result = []
	linenum = 0;
	for line in file(fname):
		linenum += 1
		line = line.strip()
		if line == '' or line[0] == '#':
			continue
		tokens = [x.strip() for x in line.split()]
		# No spaces in file names, please.
		if len(tokens) == 3:
			(src, op, dest) = tokens
			if op == '->':
				result.append(FileRemap(src, dest))
			elif op == '=>':
				result.append(DirRemap(src, dest))
			else:
				print>>sys.stderr, "Line %d: Warning: Unknown operator '%s'" % (linenum, op)
		else:
			print>>sys.stderr, "Line %d: Warning: Unparsable line" % linenum
	return result

def usage():
	print>>sys.stderr,"""Usage:
	
	%s [filenames]
	
Where [filenames] are the names of the resource modification scripts. Each
line in a modification script has the form

	old_filename -> new_filename
	
or

	old_dirname => new_dirname

This script will then rewrite resources.csv and translate.sh appropriately.
The translate.sh script should be run from the base content directory.""" % sys.argv[0]

def go(filenames):
	r = read_csv()
	shellscript = []
	for f in sys.argv[1:]:
		try:
			vals = load_script(f)
			for x in vals:
				shellscript.extend(x.svn_shell())
				for y in r:
					x.transform(y)
			print
		except IOError, msg:
			print "%s: %s" % (f, msg.strerror)
	outf = file('transform.sh', 'wt')
	for x in shellscript:
		print>>outf, x
	outf.close()
	os.chmod("transform.sh", 0755)
	outf = file('resources.csv', 'wt')
	for x in r:
		print>>outf, x.csv_line()
	outf.close()

if __name__ == '__main__':
	if len(sys.argv) == 1:
		usage()
	else:
		go(sys.argv[1:])
