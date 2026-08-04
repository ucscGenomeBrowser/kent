#!/bin/bash
# popEVE hg38 build — extract, sort, color anchors, convert, bigBed.
set -e
export PATH=$PATH:$HOME/bin/x86_64
cd /hive/data/genomes/hg38/bed/popEve

KS=$HOME/kent/src/hg/makeDb/scripts/popEve
VCF=input/grch38_popEVE_ukbb_20250715.vcf.gz
CHROMSIZES=/hive/data/genomes/hg38/chrom.sizes

echo "[$(date +%T)] 1. build NP_->strand map from ncbiRefSeq"
hgsql hg38 -N -e "select distinct l.protAcc, g.chrom, g.strand from ncbiRefSeqLink l \
  join ncbiRefSeq g on g.name=l.mrnaAcc where l.protAcc like 'NP\_%'" > np_chrom_strand.tsv
python3 -c "
prim=set(l.split()[0] for l in open('$CHROMSIZES'))
best={}
for line in open('np_chrom_strand.tsv'):
    p,c,s=line.rstrip('\n').split('\t')
    isPrim = c in prim and '_' not in c
    cur=best.get(p)
    if cur is None or (isPrim and not cur[1]):
        best[p]=(s,isPrim)
open('np_strand.tsv','w').writelines(p+'\t'+s+'\n' for p,(s,_) in best.items())
print('NP_->strand entries:',len(best))
"

echo "[$(date +%T)] 2. extract records from VCF"
zcat $VCF | python3 $KS/extractPopEve.py > popEve_records.tsv 2> extract.log
tail -1 extract.log
wc -l popEve_records.tsv

echo "[$(date +%T)] 3. compute color anchors (p0.5 / p99.5 of popEVE over all records)"
python3 -c "
import sys
vals=[]
for line in open('popEve_records.tsv'):
    vals.append(float(line.split('\t')[7]))
vals.sort()
n=len(vals)
def pct(p):
    import math
    k=(n-1)*p/100.0; f=math.floor(k); c=math.ceil(k)
    return vals[f] if f==c else vals[f]*(c-k)+vals[c]*(k-f)
lo=round(pct(0.5),3); hi=round(pct(99.5),3)
open('anchors.txt','w').write('%s %s\n'%(lo,hi))
print('p0.5=%.3f  p50=%.3f  p99.5=%.3f  -> loAnchor=%s hiAnchor=%s'%(pct(0.5),pct(50),pct(99.5),lo,hi))
"
read LO HI < anchors.txt
echo "anchors: lo=$LO hi=$HI"

echo "[$(date +%T)] 4. external sort by protein, then genomic position"
sort -t$'\t' -k1,1 -k4,4n -S 4G -T /hive/data/genomes/hg38/bed/popEve/sorttmp \
  popEve_records.tsv > popEve_sorted.tsv

echo "[$(date +%T)] 5. convert to heatmap BED"
python3 $KS/vcfToPopEveHeatmap.py popEve_sorted.tsv np_strand.tsv popEve_raw.bed $LO $HI 2> convert.log
tail -3 convert.log

echo "[$(date +%T)] 6. bedSort + filter to chrom.sizes"
bedSort popEve_raw.bed popEve_sorted_bed.bed
awk 'NR==FNR{ok[$1]=1;next} $1 in ok' $CHROMSIZES popEve_sorted_bed.bed > popEve_filtered.bed
echo "raw $(wc -l < popEve_raw.bed)  filtered $(wc -l < popEve_filtered.bed)"

echo "[$(date +%T)] 7. bedToBigBed"
bedToBigBed -type=bed12+ -tab -as=$KS/popEve_heatmap.as \
  popEve_filtered.bed $CHROMSIZES popEve.bb
ls -l popEve.bb

echo "[$(date +%T)] 8. total amino-acid positions (sum of blockCount)"
awk -F'\t' '{s+=$10} END{print "proteins:",NR,"  AA positions:",s}' popEve_filtered.bed

echo "[$(date +%T)] DONE"
