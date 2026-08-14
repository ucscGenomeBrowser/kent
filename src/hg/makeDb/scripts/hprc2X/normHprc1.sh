#!/bin/bash
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X; TB=/hive/data/genomes/hg38/hg38.2bit
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:$PATH
python3 ~/kent/src/hg/makeDb/scripts/hprc2X/leftNormDel.py $TB $D/hprc1Del.in 4 \
  | awk 'BEGIN{OFS="\t"}{k=$1":"$2":"$4; c[k]+=$5} END{for(x in c)print x,c[x]}' \
  | sort > $D/hprc1.canon.tsv
echo "DONE canonical rel1 deletion events: $(wc -l < $D/hprc1.canon.tsv)"
