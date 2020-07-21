#!/bin/bash

EVALUATION_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
ROOT_DIR="$(dirname ${EVALUATION_DIR})"
WORDLISTS_DIR="${ROOT_DIR}/wordlists"
JUNK_DIR="${EVALUATION_DIR}/junk"

RULE_BEST64="${WORDLISTS_DIR}/best64.rule"
TOP_304k="${WORDLISTS_DIR}/Top304Thousand-probable-v2.txt"
TOP_29M="${WORDLISTS_DIR}/Top29Million-probable-v2.txt.sorted"
TOP_304k_best64="${WORDLISTS_DIR}/Top304Thousand-probable-v2.txt.best64"

MATCHES_COMBINED="${EVALUATION_DIR}/matches.combined"

mkdir -p ${EVALUATION_DIR}
mkdir -p ${JUNK_DIR}

function count-lines() {
  local cnt="$(wc -l $1 | cut -d' ' -f1)"
  echo $cnt
}


if [ ! -f ${TOP_29M} ]; then
    wget -O "${WORDLISTS_DIR}/Top29Million-probable-v2.txt.gz" https://download.weakpass.com/wordlists/1857/Top29Million-probable-v2.txt.gz
    gunzip -d "${WORDLISTS_DIR}/Top29Million-probable-v2.txt.gz"
    sort "${WORDLISTS_DIR}/Top29Million-probable-v2.txt" > ${TOP_29M}
fi

if [ ! -f ${RULE_BEST64} ]; then
    wget -q -O ${RULE_BEST64} https://github.com/hashcat/hashcat/raw/master/rules/best64.rule
fi

if [ ! -f ${TOP_304k_best64} ]; then
    hashcat --stdout -r ${RULE_BEST64} ${TOP_304k} | sort -u > ${TOP_304k_best64}
fi

comm -12 ${TOP_304k_best64} ${TOP_29M} | wc -l > "${EVALUATION_DIR}/matches.best64"


cd ${ROOT_DIR}
gcc -O1 -o create_masks.o src/create_masks.c
gcc -O1 -o generate.o src/generate.c

./create_masks.o names/names.all ${TOP_304k}

max_words=$(count-lines ${TOP_304k_best64})
total_names=$(count-lines ${ROOT_DIR}/names/names.count)

names_take=$(( ${total_names} * 97 / 100 ))
awk "{if (FNR<=${names_take}) print \$2}" names/names.count > "${JUNK_DIR}/names.head"
hashcat --stdout -r ${RULE_BEST64} "${JUNK_DIR}/names.head" | sort -u > "${JUNK_DIR}/names.best64"

dump_combined="${JUNK_DIR}/combined"
dump_combined_unique="${JUNK_DIR}/combined.unique"

rm -f ${MATCHES_COMBINED}

for n_percentile in $(seq 10 20 200); do
  n=$(( ${n_percentile} * ${max_words} / 100 ))
  echo -ne "n_percentile $n_percentile \r"
  rm -f ${dump_combined}

  ./generate.o -n ${n} names/names.count masks/masks.count -q 100 >> ${dump_combined}
  ./generate.o -s -n ${n} names/names.count masks/masks.count.single -q 97 >> ${dump_combined}
  ./generate.o -s -n ${n} "${JUNK_DIR}/names.best64" masks/masks.count.single >> ${dump_combined}
  ./generate.o -n ${n} "${JUNK_DIR}/names.best64" masks/masks.count >> ${dump_combined}

  sort -u ${dump_combined} > ${dump_combined_unique}
  n_combined=$(count-lines ${dump_combined_unique})
  hits="$(comm -12 ${dump_combined_unique} ${TOP_29M} | wc -l)"

  comm -2 ${dump_combined_unique} ${TOP_304k_best64} | sort > "${JUNK_DIR}/on_top_best64"
  n_on_top=$(count-lines "${JUNK_DIR}/on_top_best64")
  hits_on_top_best64="$(comm -12 "${JUNK_DIR}/on_top_best64" ${TOP_29M} | wc -l)"

  echo "${n} ${n_combined} ${hits} ${n_on_top} ${hits_on_top_best64}" >> ${MATCHES_COMBINED}

done

rm -rf ${JUNK_DIR}

echo "Wrote statistics in ${MATCHES_COMBINED}"
