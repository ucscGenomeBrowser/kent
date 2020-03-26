#!/bin/bash
set -beEu -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/nextstrainNcov/doUpdate.sh

ottoDir=/hive/data/outside/otto/nextstrainNcov
chromSizes=/hive/data/genomes/wuhCor1/chrom.sizes
gbdbDir=/gbdb/wuhCor1/nextstrain

cd $ottoDir

# The file is named .json, but is actually gzipped, so gunzip it.
curl -s http://data.nextstrain.org/ncov.json | gunzip -c > ncov.json
curl -s -I http://data.nextstrain.org/ncov.json \
| grep Last-Mod | sed -re 's/Last-Modified: //; s/\r//;' \
   > ncov.json.date
latestDate=$(cat ncov.json.date)
oldDate=$(cat old.ncov.json.date)
if [ $(date -d "$latestDate" +%s) -le $(date -d "$oldDate" +%s)  ]; then
    exit 0
fi

today=`date +%F`
mkdir -p $today
cp -p ncov.json $today
mv ncov.json old.ncov.json
mv ncov.json.date old.ncov.json.date

runDir=$ottoDir/$today
cd $runDir

#Generate bed and VCF files
$ottoDir/nextstrain.py

# bgzip & tabix the VCF
bgzip -f nextstrainSamples.vcf
tabix -p vcf nextstrainSamples.vcf.gz

# bigBed-ify the gene names and "clades"
bedToBigBed -type=bed4 -tab -verbose=0 nextstrainGene.bed $chromSizes \
    nextstrainGene.bb

sort -k2n,2n nextstrainClade.bed > nextstrainClade.sorted.bed
bedToBigBed -as=$ottoDir/nextstrainClade.as -type=bed12+2 -tab -verbose=0 \
    nextstrainClade.sorted.bed $chromSizes \
    nextstrainClade.bb

# Archive
mkdir -p $ottoDir/archive/$today
cp -p $runDir/nextstrainGene.bb $runDir/nextstrainClade.bb $runDir/nextstrainSamples.vcf.gz{,.tbi} \
    $ottoDir/archive/$today
cp -p $runDir/ncov.json $ottoDir/archive/$today

# Install
ln -sf $runDir/nextstrainGene.bb $runDir/nextstrainClade.bb $runDir/nextstrainSamples.vcf.gz{,.tbi} \
    $gbdbDir/

echo "Updated nextstrain/ncov `date` (ncov.json date $latestDate)"
