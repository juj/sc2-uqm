#!/bin/sh

UNSTR="./unstr"
INPREFIX="./"
OUTPREFIX="./dialog/"
INFORMAT='${INPREFIX}${RACE1}.con/$ID.out'
OUTFORMAT='${OUTPREFIX}${RACE2}.txt'


while read RACE1 ID RACE2; do
	eval INFILE="$INFORMAT"
	eval OUTFILE="$OUTFORMAT"
	echo "Processing... $INFILE ==> $OUTFILE"
	$UNSTR -v "$INFILE" > "$OUTFILE"
done << EOF
	arilou   00200007  arilou
	blackur  00200007  blackur
	chmmr    00200007  chmmr
	comandr  00200007  comandr
	comandr  00400107  starbas
	druuge   00200007  druuge
	ilwrath  00200007  ilwrath
	melnorm  00200007  melnorm
	mycon    00200007  mycon
	orz      00200007  orz
	pkunk    00200007  pkunk
	shofixt  00200007  shofixt
	slyhome  00200007  slyhome
	slyland  00200007  slyland
	spathi   00200007  spathi
	spathi   00400107  spahome
	supox    00200007  supox
	syreen   00200007  syreen
	talkpet  00200007  talkpet
	thradd   00200007  thradd
	umgah    00200007  umgah
	urquan   00200007  urquan
	utwig    00200007  utwig
	vux      00200007  vux
	yehat    00200007  yehat
	yehat    00400107  rebel
	zoqfot   00200007  zoqfot
EOF

