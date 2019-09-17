kentSrc = ../../..
include ../../../inc/common.mk

PROG = bamToPsl

test:
	${PROG} chr9.NM_020469.2.blat.bam testOut.psl
	@diff expectedOut.psl testOut.psl
	${MAKE} clean

clean:
	rm -f testOut.psl

# the bam file was constructed:

# twoBitToFa /gbdb/hg38/hg38.2bit:chr9 hg38.chr9.fa
# wget -O NM_020469.2.ABO.fa \
#    "http://www.ncbi.nlm.nih.gov/sviewer/viewer.fcgi?db=nuccore&dopt=fasta&sendto=on&id=NM_020469.2"

# blat hg38.chr9.fa NM_020469.2.ABO.fa chr9.NM_020469.2.blat.psl
# pslToBed chr9.NM_020469.2.blat.psl chr9.NM_020469.2.blat.bed
# /cluster/bin/bedtools/bedtools bedtobam -bed12 -i chr9.NM_020469.2.blat.bed \
#    -g /hive/data/genomes/hg38/chrom.sizes > chr9.NM_020469.2.blat.bam
