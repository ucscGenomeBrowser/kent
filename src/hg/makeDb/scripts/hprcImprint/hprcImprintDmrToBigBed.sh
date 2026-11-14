#!/bin/bash
# Convert the lifted HPRC2 DMR bed file of Kim et al. to a bigBed with a signed methylation
# difference field, and rewrite the lifted mean methylation difference bigWig.
#
# Input bed: chrom start end Maternal|Paternal absDiff . thickStart thickEnd #color
# The README says that column 5 is the signed maternal minus paternal difference, but in the
# file it is always positive, the direction is only in column 4. So the sign is taken from
# column 4. Colors: strong (|diff| >= 0.15) and moderate, as the authors' classification,
# in the red/blue of the Imprinting collection.
set -euo pipefail
d=/hive/data/genomes/hg38/bed/imprinting/phillipy
cd $d
zcat dmrs.N05_filt.all_chr.1e3.mrg.forPlotting.grch38.mrg.coordinate_fix.bed.gz \
 | awk -F'\t' -v OFS='\t' '
   $4!="Maternal" && $4!="Paternal" {print "unexpected direction: "$0 > "/dev/stderr"; exit 1}
   $5<0 {print "unexpected negative difference: "$0 > "/dev/stderr"; exit 1}
   {
     mat = ($4=="Maternal")
     diff = mat ? $5 : -$5
     strong = ($5>=0.15)
     if (mat) c = strong ? "220,20,20" : "240,140,140"
     else     c = strong ? "0,60,200"  : "130,160,235"
     printf "%s\t%d\t%d\t%s\t0\t.\t%d\t%d\t%s\t%s\t%s\t%.3f\n",$1,$2,$3,"",$2,$3,c,$4,(strong?"Strong":"Moderate"),diff
   }' \
 | sort -k1,1 -k2,2n > hprcDmr.bed
bedToBigBed -type=bed9+3 -tab -as=$HOME/kent/src/hg/makeDb/scripts/hprcImprint/hprcImprint.as \
   hprcDmr.bed /hive/data/genomes/hg38/chrom.sizes hprcDmr.bb
wc -l hprcDmr.bed

# The CrossMap bigWig is 1 GB for 26 million bases, rewriting it with bedGraphToBigWig makes it
# much smaller, the values are unchanged.
bigWigToBedGraph all_chr.missingrate_5.createDiffDatasetFromMethylDataset.flipped.mean.grch38.bw stdout \
  | sort -k1,1 -k2,2n > hprcMethDiff.bedGraph
bedGraphToBigWig hprcMethDiff.bedGraph /hive/data/genomes/hg38/chrom.sizes hprcMethDiff.bw
rm hprcMethDiff.bedGraph
