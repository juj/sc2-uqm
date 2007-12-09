#!/usr/bin/python
import sys
import os
import os.path
import explorer
import stridgen

from optparse import OptionParser

def process (res):
    result = []
    for line in res:
	line = line.strip()
	if len(line) == 0:
            continue
        mapline = stridgen.makeid(line)
        # Remap not to .lst files but to .ls2 ones.
        if line.endswith('lst'):
            line = os.path.splitext(line)[0]+'.ls2'
        if mapline is None:
            toadd = "# NO MATCH FOR %s" % line
        else:
            toadd = "%s = %s" % (mapline, line)
        if toadd not in result:
            result.append(toadd)
    result.sort()
    return result

def get_lists(d, ext):
    result = []
    packages = [explorer.read_list(x) for x in explorer.collect_extension(d, ext)]
    for package in packages:
        for line in package:
            if line[1] not in result:
                result.append(line[1])
    return result

def get_resources(d, ext):
    result = []
    if not d.endswith(os.path.sep):
        d += os.path.sep
    for res in explorer.collect_extension (d, ext):
        res = res[len(d):]
        if res not in result:
            result.append(res)
    return result

def create_map(resources):
    resmap = process(resources)
    keys = []
    result = []
    for r in resmap:
        key = r.split('=')[0].strip()
        if key in keys:
            result.append("# ERROR: DUPLICATE KEY %s" % key)
        else:
            keys.append(key)
        result.append(r)
    return result

if __name__ == "__main__":
    opts = OptionParser(usage="usage: %prog [options]")
    opts.add_option("-d", "--content-dir", dest="d",
                    help="Directory to search for resources",
                    default=os.path.join(os.path.pardir, os.path.pardir, "sc2", "content"))
    opts.add_option("-r", "--raw", action="store_true", default=False,
                    dest="raw", help="do not treat target resources as lists")
    opts.add_option("-e", "--extension", dest="ext",
                    help="Extension for files to search", default=".lst")

    (options, args)=opts.parse_args()

    if options.raw:
        l = get_resources(options.d, options.ext)
    else:
        l = get_lists(options.d, options.ext)

    for line in create_map(l):
        print line
