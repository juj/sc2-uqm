#!/bin/sh
# Collects resource info from resinst.h files
# By Serge van den Boom, 2002-10-25
# The GPL applies.

if [ $# -ne 3 ]; then
	echo "Usage: collectres.sh <pctop> <3dotop> <destpath>"
	echo "pctop is the dir src/sc2code/ from an UQM cvs tree"
	echo "3dotop is the starcon2/ dir from the 3DO source"
	echo "destpath is a path to a dir to put the resinfo files"
	exit 1;
fi

TOPPC="${1%/}"/
TOP3DO="${2%/}"/
DEST="${3%/}"/

PARSERES=./parseres

if [ ! -e "$TOPPC"resinst.h ]; then
	echo "\"${TOPPC}\" is not a valid pc SC2 source dir"
	exit 1
fi

if [ ! -e "$TOP3DO"resinst.h ]; then
	echo "\"${TOP3DO}\" is not a valid 3DO SC2 source dir"
	exit 1
fi

if [ ! -d "$DEST" -o ! -w "$DEST" ]; then
	echo "\"${DEST}\" is not a good destination"
	exit 1
fi

export DEST
parseone() {
	$PARSERES "$1" > "${DEST}$2.res"
}

parseone "${TOPPC}resinst.h"                 starcon.pkg.pc
parseone "${TOPPC}ships/androsyn/resinst.h"  androsyn.shp.pc
parseone "${TOPPC}ships/arilou/resinst.h"    arilou.shp.pc
parseone "${TOPPC}ships/blackurq/resinst.h"  blackurq.shp.pc
parseone "${TOPPC}ships/chenjesu/resinst.h"  chenjesu.shp.pc
parseone "${TOPPC}ships/chmmr/resinst.h"     chmmr.shp.pc
parseone "${TOPPC}ships/druuge/resinst.h"    druuge.shp.pc
parseone "${TOPPC}ships/human/resinst.h"     human.shp.pc
parseone "${TOPPC}ships/ilwrath/resinst.h"   ilwrath.shp.pc
parseone "${TOPPC}ships/lastbat/resinst.h"   lastbat.sc2.pc
parseone "${TOPPC}ships/melnorme/resinst.h"  melnorme.shp.pc
parseone "${TOPPC}ships/mmrnmhrm/resinst.h"  mmrnmhrm.shp.pc
parseone "${TOPPC}ships/mycon/resinst.h"     mycon.shp.pc
parseone "${TOPPC}ships/orz/resinst.h"       orz.shp.pc
parseone "${TOPPC}ships/pkunk/resinst.h"     pkunk.shp.pc
parseone "${TOPPC}ships/probe/resinst.h"     probe.sc2.pc
parseone "${TOPPC}ships/shofixti/resinst.h"  shofixti.shp.pc
parseone "${TOPPC}ships/sis_ship/resinst.h"  sis.dat.pc
parseone "${TOPPC}ships/slylandr/resinst.h"  slylandr.shp.pc
parseone "${TOPPC}ships/spathi/resinst.h"    spathi.shp.pc
parseone "${TOPPC}ships/supox/resinst.h"     supox.shp.pc
parseone "${TOPPC}ships/syreen/resinst.h"    syreen.shp.pc
parseone "${TOPPC}ships/thradd/resinst.h"    thradd.shp.pc
parseone "${TOPPC}ships/umgah/resinst.h"     umgah.shp.pc
parseone "${TOPPC}ships/urquan/resinst.h"    urquan.shp.pc
parseone "${TOPPC}ships/utwig/resinst.h"     utwig.shp.pc
parseone "${TOPPC}ships/vux/resinst.h"       vux.shp.pc
parseone "${TOPPC}ships/yehat/resinst.h"     yehat.shp.pc
parseone "${TOPPC}ships/zoqfot/resinst.h"    zoqfot.shp.pc
parseone "${TOPPC}comm/arilou/resinst.h"     arilou.con.pc
parseone "${TOPPC}comm/blackur/resinst.h"    blackur.con.pc
parseone "${TOPPC}comm/chmmr/resinst.h"      chmmr.con.pc
parseone "${TOPPC}comm/comandr/resinst.h"    comandr.con.pc
parseone "${TOPPC}comm/druuge/resinst.h"     druuge.con.pc
parseone "${TOPPC}comm/ilwrath/resinst.h"    ilwrath.con.pc
parseone "${TOPPC}comm/melnorm/resinst.h"    melnorm.con.pc
parseone "${TOPPC}comm/mycon/resinst.h"      mycon.con.pc
parseone "${TOPPC}comm/orz/resinst.h"        orz.con.pc
parseone "${TOPPC}comm/pkunk/resinst.h"      pkunk.con.pc
parseone "${TOPPC}comm/shofixt/resinst.h"    shofixt.con.pc
parseone "${TOPPC}comm/slyhome/resinst.h"    slyhome.con.pc
parseone "${TOPPC}comm/slyland/resinst.h"    slyland.con.pc
parseone "${TOPPC}comm/spathi/resinst.h"     spathi.con.pc
parseone "${TOPPC}comm/supox/resinst.h"      supox.con.pc
parseone "${TOPPC}comm/syreen/resinst.h"     syreen.con.pc
parseone "${TOPPC}comm/talkpet/resinst.h"    talkpet.con.pc
parseone "${TOPPC}comm/thradd/resinst.h"     thradd.con.pc
parseone "${TOPPC}comm/umgah/resinst.h"      umgah.con.pc
parseone "${TOPPC}comm/urquan/resinst.h"     urquan.con.pc
parseone "${TOPPC}comm/utwig/resinst.h"      utwig.con.pc
parseone "${TOPPC}comm/vux/resinst.h"        vux.con.pc
parseone "${TOPPC}comm/yehat/resinst.h"      yehat.con.pc
parseone "${TOPPC}comm/zoqfot/resinst.h"     zoqfot.con.pc

parseone "${TOP3DO}resinst.h"                starcon.pkg.3do
parseone "${TOP3DO}androsyn/resinst.h"       androsyn.shp.3do
parseone "${TOP3DO}arilou/resinst.h"         arilou.shp.3do
parseone "${TOP3DO}blackurq/resinst.h"       blackurq.shp.3do
parseone "${TOP3DO}chenjesu/resinst.h"       chenjesu.shp.3do
parseone "${TOP3DO}chmmr/resinst.h"          chmmr.shp.3do
parseone "${TOP3DO}druuge/resinst.h"         druuge.shp.3do
parseone "${TOP3DO}human/resinst.h"          human.shp.3do
parseone "${TOP3DO}ilwrath/resinst.h"        ilwrath.shp.3do
parseone "${TOP3DO}lastbat/resinst.h"        lastbat.sc2.3do
parseone "${TOP3DO}melnorme/resinst.h"       melnorme.shp.3do
parseone "${TOP3DO}mmrnmhrm/resinst.h"       mmrnmhrm.shp.3do
parseone "${TOP3DO}mycon/resinst.h"          mycon.shp.3do
parseone "${TOP3DO}orz/resinst.h"            orz.shp.3do
parseone "${TOP3DO}pkunk/resinst.h"          pkunk.shp.3do
parseone "${TOP3DO}probe/resinst.h"          probe.sc2.3do
parseone "${TOP3DO}shofixti/resinst.h"       shofixti.shp.3do
parseone "${TOP3DO}sis_ship/resinst.h"       sis.dat.3do
parseone "${TOP3DO}slylandr/resinst.h"       slylandr.shp.3do
parseone "${TOP3DO}spathi/resinst.h"         spathi.shp.3do
parseone "${TOP3DO}supox/resinst.h"          supox.shp.3do
parseone "${TOP3DO}syreen/resinst.h"         syreen.shp.3do
parseone "${TOP3DO}thradd/resinst.h"         thradd.shp.3do
parseone "${TOP3DO}umgah/resinst.h"          umgah.shp.3do
parseone "${TOP3DO}urquan/resinst.h"         urquan.shp.3do
parseone "${TOP3DO}utwig/resinst.h"          utwig.shp.3do
parseone "${TOP3DO}vux/resinst.h"            vux.shp.3do
parseone "${TOP3DO}yehat/resinst.h"          yehat.shp.3do
parseone "${TOP3DO}zoqfot/resinst.h"         zoqfot.shp.3do
parseone "${TOP3DO}comm/arilou/resinst.h"    arilou.con.3do
parseone "${TOP3DO}comm/blackur/resinst.h"   blackur.con.3do
parseone "${TOP3DO}comm/chmmr/resinst.h"     chmmr.con.3do
parseone "${TOP3DO}comm/comandr/resinst.h"   comandr.con.3do
parseone "${TOP3DO}comm/druuge/resinst.h"    druuge.con.3do
parseone "${TOP3DO}comm/ilwrath/resinst.h"   ilwrath.con.3do
parseone "${TOP3DO}comm/melnorm/resinst.h"   melnorm.con.3do
parseone "${TOP3DO}comm/mycon/resinst.h"     mycon.con.3do
parseone "${TOP3DO}comm/orz/resinst.h"       orz.con.3do
parseone "${TOP3DO}comm/pkunk/resinst.h"     pkunk.con.3do
parseone "${TOP3DO}comm/shofixt/resinst.h"   shofixt.con.3do
parseone "${TOP3DO}comm/slyhome/resinst.h"   slyhome.con.3do
parseone "${TOP3DO}comm/slyland/resinst.h"   slyland.con.3do
parseone "${TOP3DO}comm/spathi/resinst.h"    spathi.con.3do
parseone "${TOP3DO}comm/supox/resinst.h"     supox.con.3do
parseone "${TOP3DO}comm/syreen/resinst.h"    syreen.con.3do
parseone "${TOP3DO}comm/talkpet/resinst.h"   talkpet.con.3do
parseone "${TOP3DO}comm/thradd/resinst.h"    thradd.con.3do
parseone "${TOP3DO}comm/umgah/resinst.h"     umgah.con.3do
parseone "${TOP3DO}comm/urquan/resinst.h"    urquan.con.3do
parseone "${TOP3DO}comm/utwig/resinst.h"     utwig.con.3do
parseone "${TOP3DO}comm/vux/resinst.h"       vux.con.3do
parseone "${TOP3DO}comm/yehat/resinst.h"     yehat.con.3do
parseone "${TOP3DO}comm/zoqfot/resinst.h"    zoqfot.con.3do

