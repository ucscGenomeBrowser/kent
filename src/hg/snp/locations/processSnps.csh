#!/bin/csh

# Daryl Thomas
# January 13, 2003

gunzip human.b31.snp110.gz

calcFlipSnpPos seq_contig.md human.b31.snp110 human.b31.snp110.flipped

mv   human.b31.snp110 human.b31.snp110.original
gzip human.b31.snp110.original

grep RANDOM       human.b31.snp110.flipped >  snpTsc.txt
grep MIXED        human.b31.snp110.flipped >> snpTsc.txt
grep BAC_OVERLAP  human.b31.snp110.flipped >  snpNih.txt
grep OTHER        human.b31.snp110.flipped >> snpNih.txt

gzip human.b31.snp110.flipped

awk -f filter.awk snpTsc.txt > snpTsc.contig.bed
awk -f filter.awk snpNih.txt > snpNih.contig.bed

gzip snp*.txt

liftUp snpTsc.bed ../../../jkStuff/liftAll.lft warn snpTsc.contig.bed
liftUp snpNih.bed ../../../jkStuff/liftAll.lft warn snpNih.contig.bed

gzip snp*.contig.bed

hgLoadBed hg13 snpTsc snpTsc.bed
hgLoadBed hg13 snpNih snpNih.bed

gzip snp*.bed

rm bed.tab

