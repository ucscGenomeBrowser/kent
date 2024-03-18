kentSrc = ../../..
include ../../../inc/common.mk

PROG = bamToPsl

test: test1 test2

test1: mkout
	${PROG} input/chr9.NM_020469.2.blat.bam output/$@.psl
	diff expected/$@.psl output/$@.psl

test2: mkout
	${PROG} -nohead input/multimap.sam -fasta=output/$@.fa output/$@.psl
	diff expected/$@.psl output/$@.psl
	diff expected/$@.fa output/$@.fa
	pslCheck -verbose=0 -querySizes=input/multimap.qsizes output/$@.psl

mkout:
	@mkdir -p output

clean:
	rm -rf output

##
# chr9.NM_020469.2.blat.bam:
#
# twoBitToFa /gbdb/hg38/hg38.2bit:chr9 hg38.chr9.fa
# wget -O NM_020469.2.ABO.fa \
#    "http://www.ncbi.nlm.nih.gov/sviewer/viewer.fcgi?db=nuccore&dopt=fasta&sendto=on&id=NM_020469.2"
#
# blat hg38.chr9.fa NM_020469.2.ABO.fa chr9.NM_020469.2.blat.psl
# pslToBed chr9.NM_020469.2.blat.psl chr9.NM_020469.2.blat.bed
# /cluster/bin/bedtools/bedtools bedtobam -bed12 -i chr9.NM_020469.2.blat.bed \
#    -g /hive/data/genomes/hg38/chrom.sizes > chr9.NM_020469.2.blat.bam

##
# multimap.sam
# came from mimimap2 aligns of mRNA CAT transcript on hs1 to a
# Shasta ONT assembly
# 
