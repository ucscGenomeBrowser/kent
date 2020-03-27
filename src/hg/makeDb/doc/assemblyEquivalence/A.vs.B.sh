#!/bin/bash

if [ $# -ne 2 ]; then
  printf "usage: ./A.vs.B.sh <a> <b>\n" 1>&2
  printf "where <a> <b> is a pair from:\n" 1>&2
  printf "ucsc, genbank, refseq, ensembl\n" 1>&2
  printf "creates files: nearMiss.a.b.run.list nearMiss.b.a.run.list\n" 1>&2
  exit 255
fi

export A=$1
export B=$2

printf "# existing list size:\n"
wc -l nearMiss.${A}.${B}.run.list nearMiss.${B}.${A}.run.list

cut -f1 ../${A}.${B}.exact.txt | sort -u > ${A}.${B}.done.list
cut -f1 ../${B}.${A}.exact.txt | sort -u > ${B}.${A}.done.list

cut -f2 ../${B}/${B}.keySignatures.txt  | sort -u > ${B}.full.list
cut -f2 ../${A}/${A}.keySignatures.txt  | sort -u > ${A}.full.list

comm -13 ${B}.${A}.done.list ${B}.full.list > ${B}.${A}.toMatch.list
comm -13 ${A}.${B}.done.list ${A}.full.list > ${A}.${B}.toMatch.list

join -t$'\t' -2 2 ${B}.${A}.toMatch.list \
    <(sort -k2,2 ../${B}/${B}.keySignatures.txt) \
      | awk '{printf "%d\t%s\t%d\n", $4,$1,$3}' | sort -n \
         > uniqueCounts.${B}.${A}.txt

join -t$'\t' -2 2 ${A}.${B}.toMatch.list \
  <(sort -k2,2 ../${A}/${A}.keySignatures.txt) \
      | awk '{printf "%d\t%s\t%d\n", $4,$1,$3}' | sort -n \
         > uniqueCounts.${A}.${B}.txt

/cluster/home/hiram/kent/src/hg/makeDb/doc/assemblyEquivalence
~/kent/src/hg/makeDb/doc/assemblyEquivalence/createNearMissRunList.pl 10 \
   uniqueCounts.${B}.${A}.txt uniqueCounts.${A}.${B}.txt \
     > nearMiss.${A}.${B}.run.list

~/kent/src/hg/makeDb/doc/assemblyEquivalence/createNearMissRunList.pl 10 \
   uniqueCounts.${A}.${B}.txt uniqueCounts.${B}.${A}.txt \
     > nearMiss.${B}.${A}.run.list

printf "# newly created list size:\n"
wc -l nearMiss.${A}.${B}.run.list nearMiss.${B}.${A}.run.list
