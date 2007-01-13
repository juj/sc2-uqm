#!/bin/sh

if [ $# -ne 2 ]; then
	echo "Syntax: fixplanets.sh <indir> <outdir>"
	exit 1
fi

INDIR="$1"
OUTDIR="$2"

MASKIMG="$PWD/maskimg"

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
	"$MASKIMG" -vv -01 -m "$2" -o "$OUTDIR$FILE" "$INDIR$FILE"
}

for FILE in `find "$INDIR" -type f -name "*__big.0.png"`; do
	processone "$FILE" "mask_big.png"
done

for FILE in `find "$INDIR" -type f -name "*__sml.0.png"`; do
	processone "$FILE" "mask_sml.png"
done
