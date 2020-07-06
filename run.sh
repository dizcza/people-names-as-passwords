#!/bin/bash

# prepare people names
bash bash/load_valid_names.sh

# download a test wordlist
if [ ! -f "wordlists/Top304Thousand-probable-v2.txt" ]; then
    echo "Downloading Top304K wordlist"
    wget -q --directory=wordlists/ https://github.com/berzerk0/Probable-Wordlists/raw/master/Real-Passwords/Top304Thousand-probable-v2.txt
fi

echo "Creating raw masks..."
gcc -O1 -o create_masks.o src/create_masks.c
./create_masks.o gender/names/ascii.valid wordlists/Top304Thousand-probable-v2.txt

# postprocessing; create hashcat masks
bash bash/masks_statistics.sh

echo "Done."
