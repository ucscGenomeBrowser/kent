#!/bin/bash
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X; TB=/hive/data/genomes/hg38/hg38.2bit
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:$PATH
# unbounded left-normalize each collapsed event (annot col4 = count), then
# re-merge by canonical key summing counts across events that coincide.
python3 ~/kent/src/hg/makeDb/scripts/hprc2X/leftNormDel.py $TB $D/hprc2XDel.in 4 \
  | awk 'BEGIN{OFS="\t"}{k=$1":"$2":"$4; c[k]+=$5} END{for(x in c)print x,c[x]}' \
  | sort > $D/hprc2X.canon.tsv
echo "DONE canonical HPRC deletion events: $(wc -l < $D/hprc2X.canon.tsv)"
