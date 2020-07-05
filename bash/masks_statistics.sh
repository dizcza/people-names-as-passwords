#!/bin/bash

TOP_MASKS=100

CURR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
MASKS_DIR="$(dirname ${CURR_DIR})/masks"

sort -nr ${MASKS_DIR}/most_used_names.txt > ${MASKS_DIR}/most_used_names.sorted
rm ${MASKS_DIR}/most_used_names.txt

echo "Masks statistics:"
for size in "all" "wpa"; do
    for mode in "length" "single"; do
        mkdir -p ${MASKS_DIR}/${size}/${mode}/
        if [ "${mode}" == "length" ]; then
            if [ "${size}" == "all" ]; then
                # length; all
                sort ${MASKS_DIR}/masks.raw | uniq -c | sort -nr | head -n ${TOP_MASKS} > ${MASKS_DIR}/${size}/${mode}/masks.stats
            else
                # length; wpa
                awk '{ if (length($0) >= 8) print }' ${MASKS_DIR}/masks.raw | sort | uniq -c | sort -nr | head -n ${TOP_MASKS} > ${MASKS_DIR}/${size}/${mode}/masks.stats
            fi

            awk '{print $2}' ${MASKS_DIR}/${size}/${mode}/masks.stats | awk '{
              gsub("\\|", "l?"); print;
              sub("l\\?", "u?", $1); print;
              gsub("l\\?", "u?"); print
              }' > ${MASKS_DIR}/${size}/${mode}/masks.hashcat
        else
            if [ "${size}" == "all" ]; then
                # single; all
                sed -E 's/\|+/|/' ${MASKS_DIR}/masks.raw | sort | uniq -c | sort -nr | head -n ${TOP_MASKS} > ${MASKS_DIR}/${size}/${mode}/masks.stats
            else
                # single; wpa
                sed -E 's/\|+/|/' ${MASKS_DIR}/masks.raw | awk '{ if (length($0) >= 8) print }' | sort | uniq -c | sort -nr | head -n ${TOP_MASKS} > ${MASKS_DIR}/${size}/${mode}/masks.stats
            fi

            # in single mode, does not make sense to filter by length - it won't be used directly in hashcat
        fi
    done
done;

echo "  - masks/**/masks.stats: masks statistics"
echo "  - masks/**/masks.hashcat: hashcat masks (length mode only)"
