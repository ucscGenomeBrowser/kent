#!/bin/bash
# Print dbSNP-common-deletion match stats for an HPRC canonical set.
# Usage: dbsnpStats.sh <canon.tsv> <NHAP> <label>
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X
canon="$1"; NH="$2"; lab="$3"
# exact recovery (dbSNP->HPRC same key), overall HPRC->dbSNP, Pearson r(freq)
read exRec exRecPct hd overall <<<"$(awk -F'\t' -v NH=$NH '
  FNR==NR{ rec[$1]=$2; next }
  # second file = dbSnp.canon.keys ; count recovery
  { snpTot++; if($1 in rec) r++ }
  END{ printf "%d %.1f", r, 100*r/snpTot }' $canon $D/dbSnp.canon.keys)"
overall=$(awk -F'\t' '
  FNR==NR{ snp[$1]=1; next }
  { tot++; if($1 in snp) m++ }
  END{ printf "%.2f", 100*m/tot }' $D/dbSnp.canon.keys $canon)
r=$(awk -F'\t' -v NH=$NH '
  FNR==NR{ rec[$1]=$2; next }
  { if(!($1 in rec)) next; x=$2; y=rec[$1]/NH; if(y>1)y=1; n++; sx+=x; sy+=y; sxx+=x*x; syy+=y*y; sxy+=x*y }
  END{ printf "%.3f", (n*sxy-sx*sy)/sqrt((n*sxx-sx*sx)*(n*syy-sy*sy)) }' $canon $D/dbSnp.canon.freq)
w20=$(python3 $D/windowMatch.py $canon | awk '/<=20 /{gsub(/[()%]/,""); print $3}')
printf "%-14s N=%-3s events=%-8s dbSNPrec_exact=%s%% within20=%s%% HPRC->dbSNP=%s%% Pearson_r=%s\n" \
  "$lab" "$NH" "$(wc -l < $canon)" "$exRecPct" "$w20" "$overall" "$r"
