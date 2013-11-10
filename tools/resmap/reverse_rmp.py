#!/usr/bin/env python

import sys
import os.path
from gen_resfiles import *

rmp_file = file(os.path.join("../../sc2/content/uqm.rmp"))

def load_rmp(dat):
	result = {}
	for line in dat:
		try:
			(src, dest) = line.split('=', 1)
			(t, v) = dest.split(':', 1)
			result[src.strip()] = (t.strip(), v.strip())
		except ValueError:
			print>>sys.stderr, "Warning: unparseable line "+line
	return result
	
csv_data = read_csv()
rmp_data = load_rmp(rmp_file)

for elt in csv_data:
	s = elt.res_id
	t = elt.res_type
	v = elt.res_file
	if s in rmp_data and rmp_data[s] != (t, v):
		(nt, nv) = rmp_data[s]
		print>>sys.stderr, "Changing %s to %s:%s" % (s, nt, nv)
		elt.res_type = nt
		elt.res_file = nv

outfile = file("resources.csv", "wt")
for elt in csv_data:
	print>>outfile, elt.csv_line()
