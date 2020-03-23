#!/bin/bash
set -xbeEu -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/nextstrainNcov/doUpdate.sh

ottoDir=/hive/data/outside/otto/nextstrainNcov
chromSizes=/hive/data/genomes/wuhCor1/chrom.sizes
gbdbDir=/gbdb/wuhCor1/nextstrain

cd $ottoDir

curl -s http://data.nextstrain.org/ncov.json | gunzip -c > ncov.json
if [  ! ncov.json  -nt old.ncov.json ]; then
    echo "Not newer"
    exit 0
fi

today=`date +%F`
mkdir -p $today
cp -p ncov.json $today
mv ncov.json old.ncov.json

cd $today

#Generate bed and VCF files
$ottoDir/nextstrain.py

# bgzip & tabix the VCF
bgzip -f nextstrainSamples.vcf
tabix -p vcf nextstrainSamples.vcf.gz

# bigBed-ify the gene names and "clades"
bedToBigBed -type=bed4 -tab nextstrainGene.bed $chromSizes \
    nextstrainGene.bb

sort -k2n,2n nextstrainClade.bed > nextstrainClade.sorted.bed
bedToBigBed -as=$ottoDir/nextstrainClade.as -type=bed12+2 -tab \
    nextstrainClade.sorted.bed $chromSizes \
    nextstrainClade.bb

# Archive
mkdir -p $ottoDir/archive/$today
cp -p `pwd`/nextstrainGene.bb `pwd`/nextstrainClade.bb `pwd`/nextstrainSamples.vcf.gz{,.tbi} \
    $ottoDir/archive/$today
cp -p `pwd`/ncov.json $ottoDir/archive/$today

# Install
ln -sf `pwd`/nextstrainGene.bb `pwd`/nextstrainClade.bb `pwd`/nextstrainSamples.vcf.gz{,.tbi} \
    $gbdbDir/

echo "Updated nextstrain/ncov `date`"
