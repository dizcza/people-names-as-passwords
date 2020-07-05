#!/bin/bash

# prepare people names
bash bash/load_valid_names.sh

# download a test wordlist
if [ ! -f "wordlists/Top304Thousand-probable-v2.txt" ]; then
    echo "Downloading Top304K wordlist"
    wget -q --directory=wordlists/ https://github.com/berzerk0/Probable-Wordlists/raw/master/Real-Passwords/Top304Thousand-probable-v2.txt
fi

# Recommended to run only when
#   * using partial, say ascii.valid.1000, pattern file;
#   * and a small wordlist size (<10M lines).
# Otherwise, the runtime, disk IO, and disk space used while stripping the words on a large dict make the overall progress even slower.
# In these cases, full ascii.valid file and a large (>10M lines) wordlist, use the raw wordlist file (not stripped).
# Default: no stripping.
# bash bash/strip_wordlist.sh wordlists/Top304Thousand-probable-v2.txt

# compile the program
#gcc -O1 -o create_masks.o src/create_masks.c

echo "Creating raw masks..."
#./create_masks.o gender/names/ascii.valid.1000 wordlists/Top304Thousand-probable-v2.txt
python src/create_masks.py gender/names/ascii.valid wordlists/Top304Thousand-probable-v2.txt

# postprocessing; create hashcat masks
bash bash/masks_statistics.sh

echo "Done."
