#!/bin/bash

CURR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
MASKS_DIR="${CURR_DIR}/masks"

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
sort -nr masks/most_used_names.txt > masks/most_used_names.sorted
rm masks/most_used_names.txt
echo "Sorting masks by count"
echo "  length mode"
sort ${MASKS_DIR}/masks.raw | uniq -c | sort -nr > ${MASKS_DIR}/masks.count
echo "  single-char mode"
sed -E 's/\|+/|/' ${MASKS_DIR}/masks.raw | sort | uniq -c | sort -nr > ${MASKS_DIR}/masks.count.single

echo "Done."
