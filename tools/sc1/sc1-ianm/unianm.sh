#!/bin/sh

DECOMP=/home/svdb/cvs/sc2/tools/sc1-decomp/decomp
UNIANM=/home/svdb/cvs/sc2/tools/sc1-ianm/unianm

usage() {
	echo "Syntax: unanim.sh <outdir> <infile>"
}

if [ $# -lt 1 ]; then
	usage >&2
	exit 0
fi

processFile() {
	local INPATH INDIR INNAME OUTDIR TMPFILE

	INPATH=$1
	if [ "x$INPATH" = "x" ]; then
		INDIR=""
	else
		INDIR=${INPATH%%/*}
	fi
	INNAME=${INPATH##*/}
	OUTDIR=${2%/}/$INNAME
	TMPFILE=${2%/}/$INNAME.tmp

	case "$INNAME" in
		*02)
			TYPE=2
			;;
		*03)
			TYPE=3
			;;
		*04)
			TYPE=4
			;;
		*05)
			TYPE=5
			;;
		*)
			echo "Skipping file '$INNAME' -- not of a recognised graphics type."
			return 1
	esac

	mkdir -- "$OUTDIR"
	OUTDIR=$OUTDIR/

	"$DECOMP" -o "$TMPFILE" -- "$INPATH"
	"$UNIANM" -o "$OUTDIR" -t "$TYPE" -p 00200008 -- "$TMPFILE"

	rm -f -- "$TMPFILE"
}

OUTDIR=$1
shift
while [ "$#" -gt 0 ]; do
	FILE=$1
	processFile "$FILE" "$OUTDIR"
	shift
done



