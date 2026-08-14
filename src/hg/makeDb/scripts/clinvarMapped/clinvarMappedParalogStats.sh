#!/bin/bash
# clinvarMappedParalogStats.sh - report paralog coverage of the MANE Select gene set and
# log the percent-identity distribution of paralog pairs.
#
# Inputs (in <outDir>):  maneMeta.tsv, paralogPairs.tsv
# Output: <outDir>/paralogStats.log  (internal log, also echoed to stderr)
#
# Two notions of "has a paralog":
#   (a) any BioMart paralog (the paralog need not be protein-coding / MANE);
#   (b) a mappable paralog = one that is itself in our MANE Select set, i.e. a
#       gene we can actually project a variant onto.
# Usage: clinvarMappedParalogStats.sh <outDir>
set -beEu -o pipefail

outDir="${1:?usage: clinvarMappedParalogStats.sh <outDir>}"
meta="$outDir/maneMeta.tsv"
pairs="$outDir/paralogPairs.tsv"
log="$outDir/paralogStats.log"

# ENSG sets.
cut -f2 "$meta" | sort -u > "$outDir/.maneEnsg"                 # MANE Select genes
cut -f1 "$pairs" | sort -u > "$outDir/.srcWithParalog"          # any BioMart paralog

# Mappable paralogs: pair rows where BOTH source and paralog are MANE Select genes.
awk -F'\t' 'BEGIN{OFS="\t"}
  FNR==NR{mane[$1]=1; next}
  ($1 in mane) && ($3 in mane)' "$outDir/.maneEnsg" "$pairs" \
  > "$outDir/paralogPairs.mane.tsv"
cut -f1 "$outDir/paralogPairs.mane.tsv" | sort -u > "$outDir/.srcWithManeParalog"

nMane=$(wc -l < "$outDir/.maneEnsg")
nAny=$(comm -12 "$outDir/.maneEnsg" "$outDir/.srcWithParalog" | wc -l)
nMappable=$(comm -12 "$outDir/.maneEnsg" "$outDir/.srcWithManeParalog" | wc -l)
nNoAny=$((nMane - nAny))
nNoMappable=$((nMane - nMappable))
nPairsAll=$(wc -l < "$pairs")
nPairsMane=$(wc -l < "$outDir/paralogPairs.mane.tsv")

{
  echo "# clinvarMapped paralog coverage stats  ($(date '+%F %T'))"
  echo "MANE Select protein-coding genes:            $nMane"
  echo "  with >=1 BioMart paralog (any):            $nAny"
  echo "  with NO BioMart paralog:                   $nNoAny"
  echo "  with >=1 MANE-mappable paralog:            $nMappable"
  echo "  with NO MANE-mappable paralog:             $nNoMappable"
  echo "paralog pair rows (all):                     $nPairsAll"
  echo "paralog pair rows (both ends MANE Select):   $nPairsMane"
  echo
  echo "# percent-identity distribution (col5 = query->paralog %id), MANE-mappable pairs"
  echo "# bin  count   (bin = floor(percId/10)*10)"
  awk -F'\t' '{b=int($5/10)*10; c[b]++}
    END{for(i=0;i<=100;i+=10) printf "  %3d-%-3d %8d\n", i, i+9, c[i]+0}' \
    "$outDir/paralogPairs.mane.tsv"
  echo
  echo "# summary quantiles of col5 %id (MANE-mappable pairs)"
  cut -f5 "$outDir/paralogPairs.mane.tsv" | sort -n | awk '
    {a[NR]=$1}
    END{n=NR; if(n==0){print "  (none)"; exit}
      printf "  n=%d  min=%.2f  p25=%.2f  median=%.2f  p75=%.2f  max=%.2f\n",
        n, a[1], a[int(n*0.25)+1], a[int(n*0.5)+1], a[int(n*0.75)+1], a[n]}'
} | tee "$log"

rm -f "$outDir/.maneEnsg" "$outDir/.srcWithParalog" "$outDir/.srcWithManeParalog"
