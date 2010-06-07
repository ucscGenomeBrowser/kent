#!/usr/bin/awk -f 

#
# Convert genePred file to a bed file (on stdout)
#
BEGIN {
    FS="\t";
    OFS="\t";
}
{
    name=$1
    chrom=$2
    strand=$3
    start=$4
    end=$5
    cdsStart=$6
    cdsEnd=$7
    blkCnt=$8

    delete starts
    split($9, starts, ",");
    delete ends
    split($10, ends, ",");
    blkStarts=""
    blkSizes=""
    for (i = 1; i <= blkCnt; i++) {
        blkSizes = blkSizes (ends[i]-starts[i]) ",";
        blkStarts = blkStarts (starts[i]-start) ",";
    }
    
    print chrom, start, end, name, 1000, strand, cdsStart, cdsEnd, 0, blkCnt, blkSizes, blkStarts
}
