#!/bin/sh
AIF2RAW="./aif2raw"
SOX="sox"

echo "Converting all aif files in the current directory to wav files."
echo "This script looks for aif2raw in the current dir. If it's somewhere"
echo "else, edit it to point the variable AIF2RAW to the correct location."
echo "The same goes for sox, which is expected somewhere in the path."
echo "It's just supposed to work once on a specific set of files, and hence"
echo "is pretty fragile."
echo "It is assumed that all aif files have a sample rate of 11025."
echo "If this is not the case, files won't be converted correctly."
echo "The sample rate is reported, so you can see if it goes wrong."

echo "Press ENTER when ready."
read DUMMY

for FILE in *.aif; do
	echo "File $FILE"
	BASE="${FILE%%.aif}"
	"$AIF2RAW" "$FILE" "${BASE}.raw"
	"$SOX" -c 2 -r 44100 -w -s "${BASE}.raw" "${BASE}.wav"
	echo
done

