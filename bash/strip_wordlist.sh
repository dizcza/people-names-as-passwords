#!/bin/bash

CURR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
GENDER_DIR="$(dirname ${CURR_DIR})/gender"
NAMES_DIR=${GENDER_DIR}/names

if [ ! -f ${NAMES_DIR}/ascii.valid ]; then
    echo "Run ./load_valid_names.sh first."
    exit 1
fi

if [ -z "$1" ]; then
    echo "Specify the path to a wordlist file."
    exit 1
fi

wordlist=$1
wordlist_filtered="${wordlist}.stripped"

names_split_dir=${CURR_DIR}/asci.valid.split/
wordlist_split_dir=${CURR_DIR}/wordlist.split/

rm -rf ${names_split_dir} && mkdir ${names_split_dir}
rm -rf ${wordlist_split_dir} && mkdir ${wordlist_split_dir}

split -l 100 ${NAMES_DIR}/ascii.valid ${names_split_dir}
n_splits=$(ls -1 ${names_split_dir}/* | wc -l)
count=0
for chunk in ${names_split_dir}/* ; do
    grep -iF -f "${chunk}" ${wordlist} > ${wordlist_split_dir}/$(basename ${chunk})
    (( count++ ))
    progress=$(( 100*${count}/${n_splits} ))
    echo -ne "grepping names ${progress} %\r"
done
sort -u ${wordlist_split_dir}/* > ${wordlist_filtered}
rm -rf ${names_split_dir} ${wordlist_split_dir}

echo "Stripped wordlist ${wordlist_filtered}"
