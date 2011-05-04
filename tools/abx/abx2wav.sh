#!/bin/sh
ABX2WAV="./abx2wav"

echo "Converting all abx files in the current directory to wav files."
echo "This script looks for abx2wav in the current dir. If it's somewhere"
echo "else, edit it to point the variable ABX2WAV to the correct location."
echo "It's just supposed to work once on a specific set of files, and hence"
echo "is pretty fragile."
echo "It's possible that the sample rate changes within one .abx file, and"
echo "if so a warning will be printed"
echo "Press ENTER when ready."
read

for FILE in `find . -type f -name "*.[aA][bB][xX]"`; do
	echo "File $FILE"
	# This is lame and does not handle "Abx", but I
	# do not want to mess with it too much
	BASE="${FILE%%.abx}"
	BASE="${BASE%%.ABX}"
	"$ABX2WAV" "$FILE" "${BASE}.wav"
	echo
done

