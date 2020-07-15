#!/bin/bash

# prepare people names
bash bash/load_names.sh

# download a test wordlist
if [ ! -f "wordlists/Top304Thousand-probable-v2.txt" ]; then
    echo "Downloading Top304K wordlist"
    wget -q --directory=wordlists/ https://github.com/berzerk0/Probable-Wordlists/raw/master/Real-Passwords/Top304Thousand-probable-v2.txt
fi

echo "Creating raw masks..."
gcc -O1 -o create_masks.o src/create_masks.c
./create_masks.o names/names.all wordlists/Top304Thousand-probable-v2.txt

# postprocessing; sort masks by count
echo "Sorting masks by count"
echo "  length mode"
sort masks/masks.raw | uniq -c | sort -nr > masks/masks.count
echo "  single-char mode"
sed -E 's/\|+/|/' masks/masks.count | sort -k2 | awk -f bash/single-char.awk | sort -nr | column -t > masks/masks.count.single

echo "Done."
