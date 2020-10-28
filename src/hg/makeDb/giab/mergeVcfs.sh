#!/bin/bash
set -beEu -o pipefail
for db in hg38 hg19
do
    for pop in AshkenazimTrio ChineseTrio
    do
        mkdir -p /gbdb/${db}/giab/${pop}/
        pushd ${db}/${pop}/
        if [ ${pop} == "ChineseTrio" ]
        then
            bcftools merge -o merged.vcf HG005*.vcf.gz HG006.vcf.gz HG007.vcf.gz
        else
            bcftools merge -o merged.vcf HG*.vcf.gz
        fi
        bgzip -c merged.vcf > merged.vcf.gz
        tabix -p vcf merged.vcf.gz
        ln -s `pwd`/merged.vcf.* /gbdb/${db}/giab/${pop}
        popd
    done
done
