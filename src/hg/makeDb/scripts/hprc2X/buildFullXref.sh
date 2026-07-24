#!/bin/bash
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X; TB=/hive/data/genomes/hg38/hg38.2bit
BB=/gbdb/hg38/snp/dbSnp155.bb
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:$PATH
SC=~/kent/src/hg/makeDb/scripts/hprc2X
chroms=$(cut -d: -f1 $D/hprc2Del.canon.named | sort -u)   # chroms present in hprc2Del

# per-chrom: extract class=del from full dbSnp155, left-normalize carrying rs -> key<TAB>rs
one() {
  c=$1; D=/hive/data/genomes/hg38/bed/hprc2X; TB=/hive/data/genomes/hg38/hg38.2bit
  BB=/gbdb/hg38/snp/dbSnp155.bb; SC=~/kent/src/hg/makeDb/scripts/hprc2X
  export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:$PATH
  bigBedToBed $BB -chrom=$c stdout 2>/dev/null \
    | awk -F'\t' 'BEGIN{OFS="\t"}$14=="del"{print $1,$2,$3,$4}' > $D/dbsnp155del/$c.del
  python3 $SC/leftNormDel.py $TB $D/dbsnp155del/$c.del 4 \
    | awk 'BEGIN{OFS="\t"}{k=$1":"$2":"$4; if(k in m)m[k]=m[k]","$5; else m[k]=$5} END{for(x in m)print x,m[x]}' \
    | sort > $D/dbsnp155del/$c.canon.rs
  echo "$c: $(wc -l < $D/dbsnp155del/$c.del) dels -> $(wc -l < $D/dbsnp155del/$c.canon.rs) keys"
}
export -f one
echo "$chroms" | parallel -j 12 one {}

# combine all chroms; merge any duplicate keys across (shouldn't cross chroms, but safe)
sort -m $D/dbsnp155del/*.canon.rs > $D/dbSnp155.canon.rs
echo "TOTAL full-dbSnp155 canonical del keys: $(wc -l < $D/dbSnp155.canon.rs)"

# join to every hprc2Del event (left join)
join -t $'\t' -a1 -1 1 -2 1 -o '1.2,2.2,1.1' \
   $D/hprc2Del.canon.named $D/dbSnp155.canon.rs > $D/hprc2Del.xrefFull.tsv
awk -F'\t' '{tot++; if($2!="")m++} END{printf "hprc2Del events with FULL-dbSnp155 rs: %d / %d = %.2f%%\n", m, tot, 100*m/tot}' \
   $D/hprc2Del.xrefFull.tsv
echo "DONE"
