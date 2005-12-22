#!/bin/sh

if [ $# -gt 1 ]; then
	echo "Syntax: ctmap.sh [indir] |sort [>map-file]"
	echo "Creates a map of colormaps used by various images"
	exit 1
elif [ $# -ne 1 ]; then
	INDIR="."
else
	INDIR="$1"
fi

if [ ! -d "$INDIR" ]; then
	echo "\"${INDIR}\" is not a good source dir"
	exit 1
fi

INDIR="${INDIR%/}/"
OUTDIR="${OUTDIR%/}/"

processone() {
	FILE="$1"
	LAST=-1

while read IMAGE TRANSCOLOR CMAP THEREST; do
	if [ "$CMAP" -ne "$LAST" ]; then
		LAST="$CMAP"
		FCMAP="$CMAP"
		while [ "${#FCMAP}" -lt 3 ]; do FCMAP="0$FCMAP"; done
		echo $FCMAP $FILE
	fi
done < $FILE
}

for FILE in `find "$INDIR" -type f -a "(" -name "*.ani" -o -name "*.sml" -o \
		-name "*.med" -o -name "*.big" ")"`; do
	processone "$FILE"
done
