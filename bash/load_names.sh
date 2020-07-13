#!/bin/bash
# apt install konwert

MIN_NAME_LENGTH=4

CURR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
NAMES_DIR="$(dirname ${CURR_DIR})/names"
DOWNLOADS_DIR="${NAMES_DIR}/downloads"
NAMES_ARTIFACTS_DIR="${NAMES_DIR}/artifacts"
NAMES_JUNK_DIR="${NAMES_DIR}/junk"
GENDER_DIR="${DOWNLOADS_DIR}/gender"
GENDER_NAMES_DIR="${NAMES_ARTIFACTS_DIR}/gender"

mkdir -p ${DOWNLOADS_DIR}
mkdir -p ${NAMES_ARTIFACTS_DIR}
mkdir -p ${GENDER_DIR}
mkdir -p ${GENDER_NAMES_DIR}
mkdir -p ${NAMES_JUNK_DIR}


function strip_delims() {
  # split word compounds by delims
  awk -F'[ ]|[-]|[;]|[!]|[.]|[(]|[$]|[+]|[)]|['\'']' 'NF>1 {for(i=1;i<=NF;i++) print $i}' $1 | sort -u > $1.split

  # strip delims from names
  sed 's/[ -;!.('\'')$+]//g' $1 > $1.valid
}

# -------------------- GENDER START --------------------

# download and compile 'gender' program
if [ ! -f "${DOWNLOADS_DIR}/0717-182.zip" ]; then
    echo "Downloading gender.zip"
    wget -q -O "${DOWNLOADS_DIR}/0717-182.zip" https://www.dropbox.com/s/l0mskgdp1hsv04n/0717-182.zip
fi

cd ${GENDER_DIR}

rm -rf 0717-182 && unzip -q "${DOWNLOADS_DIR}/0717-182.zip" && cd 0717-182
gcc -o gender gender.c

# save country names valid for 'gender' program
./gender -statistics | grep 'Names' | cut -d':' -f1 | sed -E 's/Names from (the )?//' | cut -d'/' -f1 | sed '/other/d' > ${GENDER_NAMES_DIR}/countries.txt

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
    konwert utf8-ascii/"${abbr}" "country/${country}" >> ${NAMES_JUNK_DIR}/names-ascii.junk.tmp
  fi
done <${GENDER_NAMES_DIR}/countries.txt

# all people names in UTF-8 format
cat country/* | sort -u | sed '/Names from/d' > ${NAMES_JUNK_DIR}/names-utf8.junk

cd ${NAMES_JUNK_DIR}

strip_delims names-utf8.junk

# UTF-8 people names of any length
cat names-utf8.* | sort -u > ${GENDER_NAMES_DIR}/gender.utf8

# the best resort to convert general utf-8 to ascii
konwert utf8-ascii ${GENDER_NAMES_DIR}/gender.utf8 >> names-ascii.junk.tmp
iconv -f utf-8 -t ascii//TRANSLIT <${GENDER_NAMES_DIR}/gender.utf8 >> names-ascii.junk.tmp
sort -u names-ascii.junk.tmp > names-ascii.junk

# a fix for UNIX systems
sed -i $'s/[^[:print:]\t]//g' names-ascii.junk


# strip non-alphabetic names due to the conversion failure
sed -E "/[^a-zA-Z]/d" names-ascii.junk | tr A-Z a-z | sort -u > ${GENDER_NAMES_DIR}/gender.ascii

# -------------------- GENDER END --------------------

# -------------------- OTHER PEOPLE NAMES --------------------

cd ${NAMES_JUNK_DIR}

if [ ! -f ${DOWNLOADS_DIR}/names_ua-ru.txt ]; then
  wget -q -O ${DOWNLOADS_DIR}/names_ua-ru.txt https://github.com/dizcza/hashcat-wpa-server/raw/master/wordlists/names_ua-ru.txt
fi

if [ ! -f ${DOWNLOADS_DIR}/names-usa.zip ]; then
  echo "Downloading USA names.zip"
  wget -q -O ${DOWNLOADS_DIR}/names-usa.zip https://www.ssa.gov/oact/babynames/names.zip
fi

unzip -qo -d names-usa ${DOWNLOADS_DIR}/names-usa.zip

# take names that occurred at least 20 times
awk -F',' '{ if ($3>=20) print $1 }' names-usa/*.txt | sort -u > names.usa

cat ${GENDER_NAMES_DIR}/gender.ascii ${DOWNLOADS_DIR}/names_ua-ru.txt names.usa | tr A-Z a-z | sort -u > names.all.junk

strip_delims names.all.junk

# strip lines with non-alphabetic characters
sed -E "/[^a-zA-Z]/d" names.all.* | awk "{ if (length >= ${MIN_NAME_LENGTH}) print }" | sort -u > ${NAMES_ARTIFACTS_DIR}/people_names.all

# -------------------- PET NAMES --------------------

if [ ! -f ${DOWNLOADS_DIR}/Dog_Names.csv ]; then
  echo "Downloading pet names"
  wget -q -O ${DOWNLOADS_DIR}/Dog_Names.csv "https://data.muni.org/api/views/a9a7-y93v/rows.csv?accessType=DOWNLOAD"
fi

# skip the first line and take the first column
awk -F',' 'NR > 1 {print $1}' ${DOWNLOADS_DIR}/Dog_Names.csv > dog_names

if [ ! -f ${DOWNLOADS_DIR}/pet_top1200_names.txt.gz ]; then
  wget -q -O ${DOWNLOADS_DIR}/pet_top1200_names.txt.gz https://www.dropbox.com/s/puvfhp5whzjvl0y/pet_top1200_names.txt.gz
fi

gzip -dkf ${DOWNLOADS_DIR}/pet_top1200_names.txt.gz

cat dog_names ${DOWNLOADS_DIR}/pet_top1200_names.txt | tr A-Z a-z | sort -u > pet_names

strip_delims pet_names

sed -E "/[^a-zA-Z]/d" pet_names* | awk "{ if (length >= ${MIN_NAME_LENGTH}) print }" | sort -u > ${NAMES_ARTIFACTS_DIR}/pet_names.all

sort -u ${NAMES_ARTIFACTS_DIR}/people_names.all ${NAMES_ARTIFACTS_DIR}/pet_names.all > ${NAMES_DIR}/names.all

# cleanup
cd ${NAMES_DIR}
rm -rf ${NAMES_JUNK_DIR}

echo "Saved people and pet names in ${NAMES_DIR}/names.all"
