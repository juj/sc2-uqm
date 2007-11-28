#!/usr/bin/python

import sys
import os
import os.path
import explorer
import stridgen

def process (res):
    result = []
    for ((a, b, c), d) in res:
        mapline = stridgen.makeid(d)
        if mapline is None:
            mapline = "ERROR"
        result.append("%3d %3d %3d %s" % (a, b, c, mapline))
    return result

if __name__ == "__main__":
    if len(sys.argv) < 2:
        d = os.path.join(os.path.pardir, os.path.pardir, "sc2", "content")
    else:
        d = sys.argv[1]
    pkgnames = explorer.get_lists(d)
    packages = [explorer.read_list(x) for x in pkgnames]
    for (name, pkg) in zip(pkgnames, packages):
        newpkg = os.path.splitext(name)[0]+'.ls2'
        print newpkg
        print '-'*len(newpkg)
        newvals = process(pkg)
        for l in newvals:
            print l
        print


    
