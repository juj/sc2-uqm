#!/usr/bin/python

import scan_defines as scan

# 72-5-4 for colortable.str_cts_ -- 0x09000504L -- OOLITE_COLOR_TAB.  Goes up through	
# 202-87-2 for PLANET00_BIG_MASK_PMAP_ANIM.  Goes up through 58.
# MAROON and CRIMSON seem swapped!

planettypes = ['oolite',
	       'yttric',
	       'quasidegenerate',
	       'lanthanide',
	       'treasure',
	       'urea',
	       'metal',
	       'radioactive',
	       'opalescent',
	       'cyanic',
	       'acid',
	       'alkali',
	       'halide',
	       'green',
	       'copper',
	       'carbide',
	       'ultramarine',
	       'noble',
	       'azure',
	       'chondrite',
	       'purple',
	       'superdense',
	       'pellucid',
	       'dust',
	       'crimson',
	       'cimmerian',
	       'infrared',
	       'selenic',
	       'auric',
	       'fluorescent',
	       'ultraviolet',
	       'plutonic',
	       'rainbow',
	       'shattered',
	       'sapphire',
	       'organic',
	       'xenolithic',
	       'redux',
	       'primordial',
	       'emerald',
	       'chlorine',
	       'magnetic',
	       'water',
	       'telluric',
	       'hydrocarbon',
	       'iodine',
	       'vinylogous',
	       'ruby',
	       'magma',
	       'maroon',
	       'bluegas', # PLANET50  # (As per colorcodes)
	       'cyangas', 
	       'greengas',
	       'greygas', 
	       'orangegas', 
	       'purplegas',
	       'redgas',
	       'violetgas',
	       'yellowgas']

headersymbols = {'quasidegenerate': 'quasi_degenerate',
		 'superdense': 'super_dense',
		 'bluegas': 'blu_gas',
		 'cyangas': 'cya_gas',
		 'greengas': 'grn_gas',
		 'greygas': 'gry_gas',
		 'orangegas': 'ora_gas',
		 'purplegas': 'pur_gas',
		 'redgas': 'red_gas',
		 'violetgas': 'vio_gas',
		 'yellowgas': 'yel_gas' }

xlattabs = ['oolite',
	    'yttric',
	    'quasidegenerate',
	    'yttric',
	    'quasidegenerate',
	    'urea',
	    'metal',
	    'yttric',
	    'opalescent',
	    'quasidegenerate',
	    'yttric',
	    'yttric',
	    'yttric',
	    'urea',
	    'quasidegenerate',
	    'opalescent',
	    'urea',
	    'yttric',
	    'urea',
	    'chondrite',
	    'urea',
	    'quasidegenerate',
	    'opalescent',
	    'yttric',
	    'urea',
	    'quasidegenerate',
	    'opalescent',
	    'urea',
	    'quasidegenerate',
	    'opalescent',
	    'yttric',
	    'yttric',
	    'rainbow',
	    'shattered',
	    'sapphire',
	    'quasidegenerate',
	    'opalescent',
	    'redux',
	    'yttric',
	    'sapphire',
	    'chlorine',
	    'opalescent',
	    'chlorine',
	    'yttric',
	    'quasidegenerate',
	    'urea',
	    'opalescent',
	    'sapphire',
	    'quasidegenerate',
	    'urea',
	    'gas',
	    'gas',
	    'gas',
	    'gas',
	    'gas',
	    'gas',
	    'gas',
	    'gas',
	    'gas']
	    
colorcodes = ['blu', 'cya', 'grn', 'gry', 'ora', 'pur', 'red', 'vio', 'yel']

# androsyn -> androsynth
# blackurq -> kohrah
# human    -> earthling
# slylandr -> slylandro
# thradd   -> thraddash
# zoqfot   -> zoqfotpik

shiptypes = ['arilou',
	     'chmmr',
	     'earthling',
	     'orz',
	     'pkunk',
	     'shofixti',
	     'spathi',
	     'supox',
	     'thraddash',
	     'utwig',
	     'vux',
	     'yehat',
	     'melnorme',
	     'druuge',
	     'ilwrath',
	     'mycon',
	     'slylandro',
	     'umgah',
	     'urquan',
	     'zoqfotpik',
             'syreen',
	     'kohrah',
	     'androsynth',
	     'chenjesu',
	     'mmrnmhrm']
	     
#blackur -> kohrah
#comandr -> commander
#melnorm -> melnorme
#shofixt -> shofixti
#slyland -> slylandro
#starbas -> starbase
#talkpet -> talkingpet
#thradd  -> thraddash
#zoqfot  -> zoqfotpik
	       
convtypes = ['arilou',
	     'chmmr',
	     'commander',
	     'orz',
	     'pkunk',
	     'shofixti',
	     'spathi',
	     'supox',
	     'thraddash',
	     'utwig',
	     'vux',
	     'yehat',
	     'melnorme',
	     'druuge',
	     'ilwrath',
	     'mycon',
	     'slylandro',
	     'umgah',
	     'urquan',
	     'zoqfotpik',
	     'syreen',
	     'kohrah',
	     'talkingpet',
	     'slyhome']

def write_planet_gfx():
	vals = scan.collect_data()
	planetanibase = vals.index([x for x in vals if x[0] == 'PLANET00_BIG_MASK_PMAP_ANIM'][0])
	(pkg, inst, typ) = scan.res_decode_str(vals[planetanibase][1])
	resmap = []
	for planettype in planettypes:
		for size in ['large', 'medium', 'small']:
			print "%3d %3d %3d planet.%s.%s" % (pkg, inst, typ, planettype, size)
			resmap.append(("planet.%s.%s" % (planettype, size), vals[planetanibase][3]))
			planetanibase += 1
			inst += 1
		pkg += 1
	print
	print "---"
	print
	for (key, val) in resmap:
		print "%s = GFXRES:%s" % (key, val)

def write_planet_str():
	vals = scan.collect_data()
	planetctbase = [x for x in vals if x[0] == 'OOLITE_COLOR_TAB'][0]
	(basepkg, ctinst, typ) = scan.res_decode_str(planetctbase[1])
	xltinst = ctinst + 1
	reslist = []
	deflist = []
	rmplist = []
	pkg = basepkg
	for (planettype, xltindex) in zip(planettypes, xlattabs):
		ctres = "planet.%s.colortable" % planettype
		xltres = "planet.%s.translatetable" % planettype
		defname = planettype
		if planettype in headersymbols:
			defname = headersymbols[defname]
		defname = defname.upper()
		ctdef = "%s_COLOR_TAB" % defname
		xltdef = "%s_XLAT_TAB" % defname
		if xltindex in headersymbols:
			oldxltdef = "%s_XLAT_TAB" % headersymbols[xltindex].upper()
		else:
			oldxltdef = "%s_XLAT_TAB" % xltindex.upper()
		ctmatch = [x for x in vals if x[0] == ctdef]
		xltmatch = [x for x in vals if x[0] == oldxltdef]
		if len(ctmatch) != 1:
			raise ValueError, "Couldn't find unique %s" % ctdef
		if len(xltmatch) != 1:
			raise ValueError, "Couldn't find unique %s" % oldxltdef
		ctfile = ctmatch[0][3]
		xltfile = xltmatch[0][3]
		reslist.append("%3d %3d %3d %s" % (pkg, ctinst, typ, ctres))
		reslist.append("%3d %3d %3d %s" % (pkg, xltinst, typ, xltres))
		deflist.append("#define %s %s" % (ctdef, scan.res_encode_str(pkg, ctinst, typ)))
		deflist.append("#define %s  %s" % (xltdef, scan.res_encode_str(pkg, xltinst, typ)))
		rmplist.append("%s = STRTAB:%s" % (ctres, ctfile))
		rmplist.append("%s = STRTAB:%s" % (xltres, xltfile))
		pkg += 1
	for l in reslist:
		print l
	print
	print "---"
	print
	for l in deflist:
		print l
	print
	print "---"
	print
	for l in rmplist:
		print l
	
if __name__ == '__main__':
	write_planet_str()
