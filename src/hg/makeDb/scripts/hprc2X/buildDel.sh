#!/bin/bash
set -beEu -o pipefail
D=/hive/data/genomes/hg38/bed/hprc2X; cd $D
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:/cluster/bin/scripts:$PATH
export TMPDIR=$D/sortTmp
AS=$HOME/kent/src/hg/makeDb/scripts/hprc2X/hprc2XArrange.as
hgSizes=/hive/data/genomes/hg38/chrom.sizes
awk '($3>$2)&&($5==0)' arr/*.indel.txt | gnusort -S32G --parallel=16 -k1,1 -k2,2n -k3,3n > delSorted.txt
echo "raw deletion rows (all assemblies): $(wc -l < delSorted.txt)"
chainArrangeCollect -exact arrDel delSorted.txt stdout \
  | awk 'BEGIN{OFS="\t"}{s=$3-$2; print $1,$2,$3,$4,($5>1000?1000:$5),$6,$7,$8,"200,0,0",$10,0,$5":"s"bp",s,"Deletion relative to GRCh38<br>Size: "s" bp<br>Assemblies with deletion: "$5}' \
  | gnusort -S16G -k1,1 -k2,2n > del.bed
echo "collapsed deletion events: $(wc -l < del.bed)"
bedToBigBed -tab -type=bed9+5 -as="$AS" del.bed "$hgSizes" out/hprc2XDel.bb 2>&1 | tail -2
echo "DONE bigBed: $(bigBedInfo out/hprc2XDel.bb 2>/dev/null | grep itemCount)"
