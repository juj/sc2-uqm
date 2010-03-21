#!/usr/bin/python

import sys
import zipfile

def read_rmp(f):
    lines = f.readlines()
    result = {}
    errors = []
    for (l, i) in zip(lines, xrange(1, 99999)):
        # strip comments
        if '#' in l:
            l = l[:l.find('#')]
        # skip blank or comment lines
        if l.strip() == '':
            continue
        if '=' not in l:
            errors.append((i, "no = in resource initializer"))
            continue
        (key, x) = l.split("=", 1)
        if ':' not in x:
            errors.append((i, "no : in resource value"))
            continue
        (restype, resval) = x.split(":", 1)
        result[key.strip()] = (i, restype.strip(), resval.strip())
    return (result, errors)

def used_types(rmp):
    v = {}
    for x in rmp:
        (i, t, z) = rmp[x]
        v[t] = 1
    r = v.keys()
    r.sort()
    return r

def check_rmp(base, target):
    valid_types = used_types(base)
    print "Valid types:"
    for x in valid_types: print x
    for k in target:
        (i, t, v) = target[k]
        if k not in base:
            print "%d: new key '%s'" % (i, k)
        if t not in valid_types:
            print "%d: new type '%s'" % (i, t)

def loot_uqm(fname):
    zf = zipfile.ZipFile(fname)
    files = zf.namelist()
    rmps = [x for x in files if x.endswith(".rmp")]
    result = {}
    errors = []
    for rmp in rmps:
        (b, e) = read_rmp(zf.open(rmp))
        result.update(b)
        errors.extend(e)
        for k in b:
            (i, t, v) = b[k]
            if v.startswith("addons/"):
                v = v[7:]
            if v not in files:
                errors.append((i, "Unknown file '%s'" % v))
    return (result, errors)

if __name__ == '__main__':
    for uqm in sys.argv[1:]:
        (b, e) = loot_uqm(uqm)
        if e != []:
            for (i, x) in e:
                print "%d: %s" % (i, x)
