#!/bin/sh

DECOMP=./decomp

for FILE in *[23459a]; do
	$DECOMP -o "$FILE.out" "$FILE"
done

