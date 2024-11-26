#!/bin/bash

set -beEu -o pipefail

cd /hive/data/outside/ncbi/genomes/reports/newAsm

for C in primates mammals vertebrate invertebrate fish birds plants \
  fungi viral bacteria archaea
do
#   printf "### %s" "${C}" 1>&2
   cat all.genbank.${C}.today all.refseq.${C}.today \
     | sed -e "s/$/\t$C/;"
#   printf "\n" 1>&2
done | sort > asmId.clade.tsv

exit $?

-rw-rw-r-- 1   789452 Jul  5 09:58 all.genbank.leftOvers.today
-rw-rw-r-- 1   242189 Jul  5 09:59 all.genbank.verts.today
-rw-rw-r-- 1     3992 Jul  6 22:25 all.refseq.leftOvers.today
-rw-rw-r-- 1    18826 Jul  6 22:26 all.refseq.verts.today

-rw-rw-r-- 1    47817 Jul  5 09:59 all.genbank.primates.today
-rw-rw-r-- 1     1073 Jul  6 22:26 all.refseq.primates.today
-rw-rw-r-- 1    58214 Jul  5 09:59 all.genbank.mammals.today
-rw-rw-r-- 1     5892 Jul  6 22:26 all.refseq.mammals.today
-rw-rw-r-- 1    18573 Jul  5 09:59 all.genbank.vertebrate.today
-rw-rw-r-- 1     2495 Jul  6 22:26 all.refseq.vertebrate.today
-rw-rw-r-- 1   274518 Jul  5 09:58 all.genbank.invertebrate.today
-rw-rw-r-- 1    12474 Jul  6 22:25 all.refseq.invertebrate.today
-rw-rw-r-- 1    69079 Jul  5 09:59 all.genbank.fish.today
-rw-rw-r-- 1     5447 Jul  6 22:26 all.refseq.fish.today
-rw-rw-r-- 1    48537 Jul  5 09:59 all.genbank.birds.today
-rw-rw-r-- 1     3919 Jul  6 22:26 all.refseq.birds.today
-rw-rw-r-- 1   153732 Jul  5 09:58 all.genbank.plants.today
-rw-rw-r-- 1     5359 Jul  6 22:25 all.refseq.plants.today
-rw-rw-r-- 1   540283 Jul  5 09:58 all.genbank.fungi.today
-rw-rw-r-- 1    17039 Jul  6 22:25 all.refseq.fungi.today
-rw-rw-r-- 1  5526529 Jul  5 09:58 all.genbank.viral.today
-rw-rw-r-- 1   455026 Jul  6 22:25 all.refseq.viral.today
-rw-rw-r-- 1 66029009 Jul  5 09:58 all.genbank.bacteria.today
-rw-rw-r-- 1   566038 Jul  6 22:25 all.refseq.bacteria.today
-rw-rw-r-- 1   696536 Jul  5 09:58 all.genbank.archaea.today
-rw-rw-r-- 1    66464 Jul  6 22:25 all.refseq.archaea.today
