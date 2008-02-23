#!/usr/bin/python

import sys
import os
import os.path
import re
import explorer

if len(sys.argv) < 2:
    basedir = os.path.join(os.path.pardir, os.path.pardir, "sc2")
else:
    basedir = sys.argv[2]

contentdir = os.path.join (basedir, "content")
srcdir = os.path.join (basedir, "src")

verbose = True

def get_type(x):
    return x & 0xFF

def get_instance(x):
    return (x >> 8) & 0x1FFF

def get_package(x):
    return (x >> 21) & 0x7FF

def res_decode(r):
    return (get_package(r), get_instance(r), get_type(r))

def res_encode(pkg, instance, t):
    return ((pkg & 0x7ff) << 21) | ((instance & 0x1FFF) << 8) | (t & 0xFF)

def res_encode_str (pkg, instance, t):
    return "0x%08xL" % res_encode (pkg, instance, t)

def res_decode_str (resnum):
    return res_decode (int(long(resnum, 16)))

def parse_packages():
    result = {}
    pkgnames = explorer.collect_extension(contentdir, '.ls2')
    packages = [(x, explorer.read_list(x)) for x in pkgnames]
    for (name, pkg) in packages:
        name = name[len(contentdir)+1:]
        this_index = {}
        for ((p, i, t), target) in pkg:
            this_index[res_encode(p, i, t)] = target
        result[name] = this_index
    return result

def parse_resmap():
	result = {}
	for line in file(os.path.join(contentdir, 'uqm.rmp')):
		xs = [x.strip() for x in line.split('=', 1)]
		if len (xs) == 2:
			if ':' in xs[1]:
				vt = xs[1][:xs[1].index(':')]
				xs[1] = xs[1][xs[1].index(':')+1:]
			else:
				vt = "UNKNOWN_RES"
			result[xs[0]] = (vt, xs[1])
	return result
    
def get_index_from_header (h):
	match = re.search (r"sc2code/comm/([^/]*)/.*\.h", h)
	if match != None:
		return "comm/%s.ls2" % match.group(1)
	match = re.search (r"sc2code/ships/([^/]*)/.*\.h", h)
	if match != None:
		return "%s.ls2" % match.group(1)
	return "starcon.ls2"

def parse_headers(pkgdata, fnamemap):
	result = []
	used_indices = {}
	used_ids = {}
	headernames = explorer.collect_res_headers(srcdir)
	headerdata = [(x, explorer.read_header(x)) for x in headernames]
	for (name, defines) in headerdata:
		index = get_index_from_header (name)
                name = name[len(srcdir)+1:]
		if index in pkgdata:
			resmap = pkgdata[index]
			for (sym, val) in defines:
				try:
                                        (vp, vi, vt) = res_decode_str (val)
					trueval = res_encode (vp, vi, vt)
					used_indices[trueval]=True
					if trueval in resmap:
						res_id = resmap[trueval]
						used_ids[res_id]=True
						if res_id in fnamemap:
							(res_type, fname) = fnamemap[res_id]
						else:
							fname = '--'
							res_type = '--'
					else:
						res_id = '--'
						fname = '--'
					result.append((name, index, sym, vp,vi,vt, res_id, res_type, fname))
				except ValueError:
					# This was something that wasn't a resource index
					pass
		else:
			print>>sys.stderr, "Warning: Unknown RES_INDEX '%s'" % index
	for pkgname in pkgdata:
		pkg = pkgdata[pkgname]
		for id in pkg:
			if id not in used_indices:
				res_id = pkg[id]
				used_ids[res_id]=True
				if res_id in fnamemap:
					fname = fnamemap[res_id]
				else:
					fname = '--'
				(vp, vi, vt) = res_decode(id)
				result.append(('--', '--', '--', vp, vi, vt, res_id, fname))
	for id in fnamemap:
		if id not in used_ids:
			result.append(('--', '--', id, fnamemap[id]))
	return result

def collect_data ():
	return parse_headers (parse_packages(), parse_resmap())


def dump_html(vals):
	print """<html>
  <head>
    <title>UQM Resource Mappings as of 0.6.4</title>
  </head>
  <body>
    <h1>UQM Resource Mappings</h1>
    <p>This table lists all of the various <tt>#define</tt>s used in the UQM code, the 32-bit RESOURCE index, the values it ultimately maps to.</p>
    
    <table>
      <tr><th>Header file</th><th>Resource File</th><th>Constant</th><th>Resource Number</th><th>Resource Name</th><th>Resource Type</th><th>Default filename</th></tr>"""
	for x in vals:
		print "      <tr><td>%s</td><td>%s</td><td>%s</td><td>%d,%d,%d</td><td>%s</td><td>%s</td><td>%s</td></tr>" % x
	print """    </table>
  </body>
</html>"""   

def dump_csv(vals):
	for x in vals:
		# Header file this belongs to
		# Resource file this belongs to
		# Source constant
		# package, instance, type
		# Resource id string
		# Resource type string - must be consistant with type above
		# filename
		print "%s,%s,%s,%d,%d,%d,%s,%s,%s" % x

if __name__ == '__main__':
	vs = collect_data()
#	dump_csv(vs)
#	print "--------"
        dump_html(vs)
