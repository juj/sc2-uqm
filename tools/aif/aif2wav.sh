#!/bin/sh
AIF2WAV="./aif2wav"

echo "Converting all aif files in the current directory to wav files."
echo "This script looks for aif2wav in the current dir. If it's somewhere"
echo "else, edit it to point the variable AIF2WAV to the correct location."
echo "It's just supposed to work once on a specific set of files, and hence"
echo "is pretty fragile."

echo "Press ENTER when ready."
read DUMMY

for FILE in `find . -type f -name "*.[aA][iI][fF]"`; do
	echo "File $FILE"
	# This is lame and does not handle "Aif", but I
	# do not want to mess with it too much
	BASE="${FILE%%.aif}"
	BASE="${BASE%%.AIF}"
	"$AIF2WAV" "$FILE" "${BASE}.wav"
	echo
done

