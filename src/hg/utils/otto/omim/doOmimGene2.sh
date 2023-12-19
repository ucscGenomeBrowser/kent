#!/bin/sh -e
# doOmimGene2.sh $db omimGene2new

set -eEu -o pipefail

db=$1
output=$2

hgsql $db -Ne "select geneId, omimId from omim2geneNew where geneId <>'-' and entryType='gene' " | sort > locusToOmim.txt
hgsql $db -Ne "select r.name,r.chrom,r.txStart,r.txEnd,locusLinkId from ncbiRefSeq r,ncbiRefSeqLink where r.name=id and r.name not like 'X%'"  | awk '{print $5,$1,$2,$3,$4}' | sort > locusToId.txt

join locusToId.txt locusToOmim.txt | awk '{print $0,$5-$4}' | sort -k 1,1 -k 3,3 -k 7,7nr |  awk '{if (($1 != last1) || ($3 != last3)) print; last1=$1;last3=last$3}' | awk '{print $3,$4,$5,$6}' | sort -k 1,1 -k 2,2n | uniq > $output

rm -rf locusToId.txt locusToOmim.txt


