#!/bin/bash
# apt install konwert dos2unix

MIN_NAME_LENGTH=4

CURR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
GENDER_DIR="$(dirname ${CURR_DIR})/gender"
NAMES_DIR=${GENDER_DIR}/names

mkdir -p ${GENDER_DIR}
mkdir -p ${NAMES_DIR}

# download and compile 'gender' program
cd ${GENDER_DIR}
if [ ! -f "0717-182.zip" ]; then
    echo "Downloading gender.zip"
    wget -q https://www.dropbox.com/s/l0mskgdp1hsv04n/0717-182.zip
fi
rm -rf 0717-182 && unzip -q 0717-182.zip && cd 0717-182
gcc -o gender gender.c

# save country names valid for 'gender' program
./gender -statistics | grep 'Names' | cut -d':' -f1 | sed -E 's/Names from (the )?//' | cut -d'/' -f1 | sed '/other/d' > ${NAMES_DIR}/countries.txt

# country-specific translations
declare -A abbreviations=( ["Germany"]="de" ["Czech Republic"]="cz" ["Greece"]="el" ["Spain"]="es" ["France"]="fr" \
                           ["Israel"]="he" ["Italy"]="it" ["Poland"]="pl" ["Portugal"]="pt" ["Sweden"]="sv" )

# extract UTF-8 people names for each country
mkdir country
while read country; do
  ./gender -print_names_of_country ${country} -utf8 "country/${country}"
  abbr="${abbreviations[${country}]}"
  if [ "${abbr}" ]
  then
    # apply language rules to specific countries (i.e, German ä -> ae)
    konwert utf8-ascii/"${abbr}" "country/${country}" >> names-ascii.junk.tmp
  fi
done <${NAMES_DIR}/countries.txt

# all people names in UTF-8 format
cat country/* | sort -u | sed '/Names from/d' > names-utf8.junk

# ascii printable delimiters in names
grep -Pio '\W' names-utf8.junk | sort -u | grep -P '[\x20-\x7E]' > delims

# split word compounds by delims
awk '
    BEGIN{FS=""; while((getline < "delims") > 0){FS = FS=="" ? $0 : FS"|"$0}}
    NF>1 {for(i=1;i<=NF;i++) print $i}
' names-utf8.junk | sort -u > names-utf8.junk.split

# remove delims from names, but don't split
sed "s/.*/s\/&\/\/g/" delims > delims.sed
sed -f delims.sed names-utf8.junk > names-utf8.junk.valid

# valid UTF-8 people names with at least 4 characters
cat names-utf8.* | sort -u | awk "{ if (length >= ${MIN_NAME_LENGTH}) print }" > ${NAMES_DIR}/utf8

# the best resort to convert general utf-8 to ascii
konwert utf8-ascii ${NAMES_DIR}/utf8 >> names-ascii.junk.tmp
iconv -f utf-8 -t ascii//TRANSLIT <${NAMES_DIR}/utf8 >> names-ascii.junk.tmp
sort -u names-ascii.junk.tmp > names-ascii.junk

# a fix for UNIX systems
dos2unix -q names-ascii.junk

# strip non-alphabetic names due to the conversion failure
sed -E "/[^a-zA-Z]/d" names-ascii.junk | awk "{ if (length >= ${MIN_NAME_LENGTH}) print }" | tr A-Z a-z | sort -u > names-ascii.valid.unsorted

# sort by length in descending order
awk '{ print length, $0 }' names-ascii.valid.unsorted | sort -nr | cut -d" " -f2- > ${NAMES_DIR}/ascii.valid

tail -n1000 ${NAMES_DIR}/ascii.valid > ${NAMES_DIR}/ascii.valid.1000

# cleanup
rm -rf *.junk* names-ascii.valid.unsorted delims*

echo "Saved people names in ${NAMES_DIR}"
