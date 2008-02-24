#!/usr/bin/python

import sys
import os
import os.path
import explorer
import mastermap
import relst

if len(sys.argv) < 2:
    basedir = os.path.join(os.path.pardir, os.path.pardir, "sc2", "content")
else:
    basedir = sys.argv[1]

verbose = True

def deploy_map():
    target = os.path.join(basedir, "uqm.rmp")
    outfile = file(target, 'wt')
    if verbose:
        print "Writing %s..." % target
    for l in mastermap.create_map(basedir):
        print>>outfile, l
    outfile.close()

def deploy_ls2(name, pkg):
    newpkg = os.path.splitext(name)[0]+'.ls2'
    newvals = relst.process(pkg)
    outfile = file(newpkg, 'wt')
    print "Writing %s..." % newpkg
    for l in newvals:
        print>>outfile, l
    outfile.close()

def deploy_ls2s():
    pkgnames = explorer.get_lists(basedir)
    packages = [(x, explorer.read_list(x)) for x in pkgnames]
    for (name, pkg) in packages:
        deploy_ls2(name, pkg)


if __name__ == '__main__':
    deploy_map()
    deploy_ls2s()


