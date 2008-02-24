#!/usr/bin/python

import sys
import os
import os.path
import explorer
import mastermap
import relst

if len(sys.argv) < 3:
    basedir = os.path.join(os.path.pardir, os.path.pardir, "sc2", "content")
else:
    basedir = sys.argv[2]

if len(sys.argv) < 2:
    targetrmp = os.path.join (basedir, "uqm.rmp")
else:
    targetrmp = sys.argv[1]

verbose = True

types = ['UNKNOWN', 'KEY_CONFIG', 'GFXRES', 'FONTRES', 'STRTAB', 'SNDRES', 'MUSICRES', 'RES_INDEX', 'CODE']

def read_types():
    result = {}
    pkgnames = explorer.collect_extension(basedir, '.ls2')
    packages = [(x, explorer.read_list(x)) for x in pkgnames]
    for (name, pkg) in packages:
        for ((p, i, t), target) in pkg:
            result[target] = types[t]
    return result

def convert_map (mapfile, typemap):
    result = []
    for line in file(mapfile):
        xs = line.split('=', 1)
        if len(xs) == 2:
            resid = xs[0].strip()
            resname = typemap[resid] + ":" + xs[1].strip()
            result.append("%s = %s" % (resid, resname))
    return result

def go():
    typemap = read_types()
    for x in convert_map(targetrmp, typemap):
        print x

if __name__ == '__main__':
    go()

