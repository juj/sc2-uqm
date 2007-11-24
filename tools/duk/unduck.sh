#!/bin/sh
UNDUCK="./unduck"
SOX="sox"

echo "Extracting the sound data from all .duk files in the current directory."
echo "This script looks for unduck in the current dir. If it's somewhere"
echo "else, edit it to point the variable UNDUCK to the correct location."
echo "The same goes for sox, which is expected somewhere in the path."
echo "It is assumed that all sound files has a sample rate of 22050Hz"
echo "and are stereo."
echo "N.B. This program isn't fast. Make a cup of tea or something."

echo "Press ENTER when ready."
read DUMMY

for FILE in *.duk *.DUK; do
	if [ ! -r "$FILE" ]; then
		continue
	fi
	echo "File $FILE"
	case "$FILE" in
		*.duk)
			BASE="${FILE%.duk}"
			;;
		*.DUK)
			BASE="${FILE%.DUK}"
			;;
	esac
	"$UNDUCK" "$FILE"
	"$SOX" -c 2 -r 22050 -w -s -t raw "${BASE}.raw" -t wav "${BASE}.wav"
	rm -- "${BASE}.raw"
	echo
done

