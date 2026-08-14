#!/bin/bash

if [ $# -ne 1 ]; then
  printf "usage: queryCounts.sh <accession>\n" 1>&2
  exit 255
fi

export db="${1}"

mkdir -p queryCounts
for F in result/*.txt
do
  if [ -s "${F}" ]; then
     B=`basename "${F}"`
  grep "^${db}" "${F}" | cut -f3 | awk '{
    split($1, a, ".")
    if ($1 ~ /^GC/) {
      printf "%s\t%s.%s\n", $3, a[1], a[2]
    } else {
      printf "%s\t%s\n", $3, a[1]
    }
}' > queryCounts/${B}.t
   ~/kent/src/hg/makeDb/doc/vgp577way/sumBlocks.py queryCounts/${B}.t > queryCounts/${B}
   rm -f queryCounts/${B}.t
   printf "%s\n" "${B}"
   fi
done

cat queryCounts/* > all.counts
~/kent/src/hg/makeDb/doc/vgp577way/sumBlocks.py sumBlocks.py all.counts | sort -rn > order.list
