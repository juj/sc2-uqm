#!/bin/sh
ABX2RAW="./abx2raw"
SOX="sox"

echo "Converting all abx files in the current directory to wav files."
echo "This script looks for abx2raw in the current dir. If it's somewhere"
echo "else, edit it to point the variable ABX2RAW to the correct location."
echo "The same goes for sox, which is expected somewhere in the path."
echo "It's just supposed to work once on a specific set of files, and hence"
echo "is pretty fragile."
echo "It is assumed that all abx files have a sample rate of 11025."
echo "If this is not the case, files won't be converted correctly."
echo "The sample rate is reported, so you can see if it goes wrong."
echo "It's also possible that the sample rate changes within one .abx file."

echo "Press ENTER when ready."
read

for FILE in *.abx; do
	echo "File $FILE"
	BASE="${FILE%%.abx}"
	"$ABX2RAW" "$FILE" "${BASE}.raw"
	"$SOX" -c 1 -r 11025 -b -s "${BASE}.raw" "${BASE}.wav"
	echo
done

