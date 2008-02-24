#!/usr/bin/python

import sys
import os
import os.path

basedir = os.path.join(os.path.pardir, os.path.pardir, "sc2")
contentdir = os.path.join (basedir, "content")
srcdir = os.path.join (basedir, "src")

class ResEntry(object):
	def __init__(self, line_num, hfile, ls2file, constant, pkg, inst, typ, res_id, res_type, res_file):
		self.line_num = line_num
		self.hfile = hfile
		self.ls2file = ls2file
		self.constant = constant
		self.package = int(pkg)
		self.instance = int(inst)
		self.int_type = int(typ)
		self.res_id = res_id
		self.res_type = res_type
		self.res_file = res_file
	def res_number(self):
		return res_encode (self.package, self.instance, self.int_type)

def res_encode(pkg, instance, t):
    return ((pkg & 0x7ff) << 21) | ((instance & 0x1FFF) << 8) | (t & 0xFF)

def res_encode_str (pkg, instance, t):
    return "0x%08xL" % res_encode (pkg, instance, t)

def read_csv (fname="resources.csv"):
	result = []
	linenum = 0
	for line in file(fname):
		linenum += 1
		# We allow hash comments, but it's not wise to have them in the official
		# list, since it may interfere with CSV importers.
		if '#' in line:
			line = line[:line.index('#')]
		line = line.strip()
		if len(line) > 0:
			data = [x.strip() for x in line.split (',')]
			if len(data) == 9:
				try:
					result.append(ResEntry(linenum, *data))
				except ValueError:
					print>>sys.stderr, "Warning: Noninteger P/I/T in line %d" % linenum
			else:
				print>>sys.strderr, "Warning: Incorrect number of fields in line %d" % linenum
	return result

def get_headers(l):
	result = {}
	for res in l:
		result[res.hfile]=True
	names = result.keys()
	names.sort()
	return names
		

def get_resfiles(l):
	result = {}
	for res in l:
		result[res.ls2file]=True
	names = result.keys()
	names.sort()
	return names

def ls2_text(index, vals):
	result = []
	in_index = [x for x in vals if x.ls2file == index]
	in_index.sort(lambda x,y: x.res_number() - y.res_number())
	for entry in in_index:
		result.append("%3d %3d %3d %s" % (entry.package, entry.instance, entry.int_type, entry.res_id))
	return result

def rmp_text(vals):
	result = []
	in_index = [x for x in vals if x.res_file != "--"]
	in_index.sort(lambda x,y: cmp(x.res_id, y.res_id))
	for entry in in_index:
		if x.res_type != "--":
			result.append("%s = %s:%s" % (entry.res_id, entry.res_type, entry.res_file))
		else:
			print>>sys.stderr, "Warning: Resource ID %s (on line %d) has no associated type" % (entry.line_num, entry.res_id)
			result.append("%s = UNKNOWNRES:%s" % (entry.res_id, entry.res_file))
	return result		

def h_text(header, vals):
	result = ["/* This file was auto-generated by the gen_resfiles utility and",
		  "   should not be edited directly.  Modify the master resource list",
		  "   instead and regenerate. */",
		  ""]
	in_index = [x for x in vals if x.hfile == header]
	in_index.sort(lambda x,y: x.res_number() - y.res_number())
	for entry in in_index:
		result.append("#define %s %s" % (entry.constant, res_encode_str(entry.package, entry.instance, entry.int_type)))
	return result

def dump_values(data):
	keys = data.keys()
	keys.sort()
	for f in keys:
		print "Writing ",f
		outfile = file(f,"wt")
		for l in data[f]:
			print>>outfile, l
		outfile.close()

def collate_data(data):
	result = {}
	headers = get_headers(data)
	indices = get_resfiles(data)
	for index in indices:
		result[os.path.join(contentdir, *(index.split('/')))] = ls2_text(index, data)
	for header in headers:
		result[os.path.join(srcdir, *(header.split('/')))] = h_text(header, data)
	result[os.path.join(contentdir, 'uqm.rmp')] = rmp_text(data)
	return result
	
if __name__=="__main__":
	data = collate_data(read_csv())
	dump_values(data)