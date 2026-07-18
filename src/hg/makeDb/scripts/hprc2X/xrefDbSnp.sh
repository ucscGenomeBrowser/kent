#!/bin/bash
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X; TB=/hive/data/genomes/hg38/hg38.2bit
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:$PATH
# normalize hprc2Del events (annot col4 = arrDel name) -> canonical key + name
python3 ~/kent/src/hg/makeDb/scripts/hprc2X/leftNormDel.py $TB $D/hprc2Del.pos 4 \
  | awk 'BEGIN{OFS="\t"}{print $1":"$2":"$4, $5}' | sort > $D/hprc2Del.canon.named
# join canonical key -> rs (left join: every hprc2Del event, rs or empty)
join -t $'\t' -a1 -1 1 -2 1 -o '1.2,2.2,1.1' \
   $D/hprc2Del.canon.named $D/dbSnp.canon.rs > $D/hprc2Del.xref.tsv
echo "DONE"
