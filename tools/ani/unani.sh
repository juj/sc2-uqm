#!/bin/sh

if [ $# -ne 2 ]; then
	echo "Syntax: unani.sh <indir> <outdir>"
	exit 1
fi

INDIR="$1"
OUTDIR="$2"

UNANI="$PWD/unani"

if [ ! -d "$INDIR" ]; then
	echo "\"${INDIR}\" is not a good source dir"
	exit 1
fi

if [ ! -d "$OUTDIR" -o ! -w "$OUTDIR" ]; then
	echo "\"${OUTDIR}\" is not a good destination"
	exit 1
fi

INDIR="${INDIR%/}/"
OUTDIR="${OUTDIR%/}/"

processone() {
	FILE="$1"
	FILE="${FILE#${INDIR}}"
	DIR="${FILE%/*}"
	echo "Now processing $FILE"
	if [ "$DIR" != "$FILE" -a ! -d "${OUTDIR}${DIR}" ]; then
		mkdir -p -- "${OUTDIR}${DIR}"
	fi
	if [ "$DIR" = "$FILE" ]; then
		DIR=""
	fi
	PREFIX="${FILE##*/}"
	PREFIX="${PREFIX%.*}"
	EXT="${FILE##*.}"
	echo "$EXT" "$PREFIX"
	if [ "$EXT" = "ani" ]; then
		"$UNANI" -c "$INDIR"lbm/scclrtab.ct -o "$OUTDIR$DIR" "$INDIR$FILE"
		"$UNANI" -bta "$INDIR$FILE" > "$OUTDIR$FILE"
	else
		"$UNANI" -c "$INDIR"lbm/scclrtab.ct -n "$PREFIX.$EXT" \
				-o "$OUTDIR$DIR" "$INDIR$FILE"
		"$UNANI" -a "$INDIR$FILE" -n "$PREFIX.$EXT" > "$OUTDIR$FILE"
	fi
	echo
}

for FILE in `find "$INDIR" -type f -a "(" -name "*.ani" -o -name "*.sml" -o \
		-name "*.med" -o -name "*.big" ")"`; do
	processone "$FILE"
done
