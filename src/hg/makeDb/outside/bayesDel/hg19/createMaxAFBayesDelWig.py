#!/usr/bin/env python3

import gzip
import sys
# File formats:
# Chr    Start   ref     alt     BayesDel_nsfp33a_noAF
# 1      69091   A       C       -0.361008

allFiles = ["BayesDel_170824_addAF_chr1.gz", "BayesDel_170824_addAF_chr2.gz",
            "BayesDel_170824_addAF_chr3.gz", "BayesDel_170824_addAF_chr4.gz",
            "BayesDel_170824_addAF_chr5.gz", "BayesDel_170824_addAF_chr6.gz",
            "BayesDel_170824_addAF_chr7.gz", "BayesDel_170824_addAF_chr8.gz",
            "BayesDel_170824_addAF_chr9.gz", "BayesDel_170824_addAF_chr10.gz",
            "BayesDel_170824_addAF_chr11.gz", "BayesDel_170824_addAF_chr12.gz",
            "BayesDel_170824_addAF_chr13.gz", "BayesDel_170824_addAF_chr14.gz",
            "BayesDel_170824_addAF_chr15.gz", "BayesDel_170824_addAF_chr16.gz",
            "BayesDel_170824_addAF_chr17.gz", "BayesDel_170824_addAF_chr18.gz",
            "BayesDel_170824_addAF_chr19.gz", "BayesDel_170824_addAF_chr20.gz",
            "BayesDel_170824_addAF_chr21.gz", "BayesDel_170824_addAF_chr22.gz",
            "BayesDel_170824_addAF_chrM.gz", "BayesDel_170824_addAF_chrX.gz", 
            "BayesDel_170824_addAF_chrY.gz"]

mutA = dict()
mutC = dict()
mutG = dict()
mutT = dict()

mutAFile = open('./hg19/hg19.MaxAF.bayesDel.mutA.wig', 'w')
mutCFile = open('./hg19/hg19.MaxAF.bayesDel.mutC.wig', 'w')
mutGFile = open('./hg19/hg19.MaxAF.bayesDel.mutG.wig', 'w')
mutTFile = open('./hg19/hg19.MaxAF.bayesDel.mutT.wig', 'w')

for file in allFiles:
    with gzip.open('/cluster/home/tmpereir/public_html/trackHubs/bayesDel/data/BayesDel_170824_addAF/'+file) as f:
        for line in f.readlines():
            line = line.decode("utf-8")
            if line.startswith('#'):
                continue
            else:
                chr, pos, ref, alt, score = line.strip().split()
                # Set up chromosome key if not encountered yet
                if chr not in mutA.keys():
                    mutA[chr] = dict()
                if chr not in mutC.keys():
                    mutC[chr] = dict()
                if chr not in mutG.keys():
                    mutG[chr] = dict()
                if chr not in mutT.keys():
                    mutT[chr] = dict()

                if alt == 'A':
                    mutA[chr][pos] = score
                elif alt == 'C':
                    mutC[chr][pos] = score
                elif alt == 'G':
                    mutG[chr][pos] = score
                elif alt == 'T':
                    mutT[chr][pos] = score

                # Add 0 score 
                if ref == 'A':
                    mutA[chr][pos] = '0'
                elif ref == 'C':
                    mutC[chr][pos] = '0'
                elif ref == 'G':
                    mutG[chr][pos] = '0'
                elif ref == 'T':
                    mutT[chr][pos] = '0'
# A mutations
#mutAFile.write('variableStep\tchrom=chr1')
#mutAFile.write('\n')
for chr in mutA.keys():
    # Write header
    mutAFile.write('variableStep\tchrom=chr'+chr)
    mutAFile.write('\n')
    for pos in mutA[chr]:
        mutAFile.write(pos+'\t'+mutA[chr][pos])
        mutAFile.write('\n')

# C mutations
for chr in mutC.keys():
    # Write header
    mutCFile.write('variableStep\tchrom=chr'+chr)
    mutCFile.write('\n')
    for pos in mutC[chr]:
        mutCFile.write(pos+'\t'+mutC[chr][pos])
        mutCFile.write('\n')

# G mutations
for chr in mutG.keys():
    # Write header
    mutGFile.write('variableStep\tchrom=chr'+chr)
    mutGFile.write('\n')
    for pos in mutG[chr]:
        mutGFile.write(pos+'\t'+mutG[chr][pos])
        mutGFile.write('\n')

# T mutations
for chr in mutT.keys():
    # Write header
    mutTFile.write('variableStep\tchrom=chr'+chr)
    mutTFile.write('\n')
    for pos in mutT[chr]:
        mutTFile.write(pos+'\t'+mutT[chr][pos])
        mutTFile.write('\n')

mutAFile.close()
mutCFile.close()
mutGFile.close()
mutTFile.close()
            
