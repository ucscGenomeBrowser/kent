#!/bin/bash

# for each trio in the ./output dir, install files into /gbdb and load up a table with the pointers
p="/gbdb/hg38/1000Genomes/trio/"
for d in output/*
do
    # the '##' below is a bashism for deleting the first leading match
    # (it deletes 'output/' from whatever is in $d)
    tbl=${d##output/}
    mkdir -p ${p}${tbl}
    cd ${d}
    ln -s `pwd`/*.vcf.gz* /gbdb/hg38/1000Genomes/trio/${tbl}
    cp /dev/null tgp${tbl}.txt
    for c in {1..22}
    do
        f=${tbl}Trio.chr${c}.vcf.gz
        echo -e "${p}${tbl}/${f}\tchr${c}" >> tgp${tbl}.txt
    done
    # don't forget chrX
    f=${tbl}Trio.chrX.vcf.gz
    echo -e "${p}${tbl}/${f}\tchrX" >> tgp${tbl}.txt
    hgLoadSqlTab hg38 tgp${tbl} ~/kent/src/hg/lib/bbiChroms.sql tgp${tbl}.txt
    cd ../..
done
