#!/bin/bash
set -beEu -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/nextstrainNcov/doUpdate.sh

ottoDir=/hive/data/outside/otto/nextstrainNcov
chromSizes=/hive/data/genomes/wuhCor1/chrom.sizes
gbdbDir=/gbdb/wuhCor1/nextstrain

ncovJsonUrl='https://nextstrain.org/charon/getDataset?prefix=/ncov/global'

cd $ottoDir

# The file is named .json, but is actually gzipped, so gunzip it.
curl -s "$ncovJsonUrl" \
| gunzip -c > ncov.json
curl -s -I "$ncovJsonUrl" \
| grep Last-Mod | sed -re 's/Last-Modified: //; s/\r//;' \
   > ncov.json.date
latestDate=$(cat ncov.json.date)
oldDate=$(cat old.ncov.json.date)
if [ $(date -d "$latestDate" +%s) -le $(date -d "$oldDate" +%s)  ]; then
    exit 0
fi

today=`date +%F`
mkdir -p $today
ncovTime=$(date -d "$latestDate" +%F-%H:%M)
cp -p ncov.json $today/ncov.$ncovTime.json
mv ncov.json old.ncov.json
mv ncov.json.date old.ncov.json.date

runDir=$ottoDir/$today
cd $runDir
chmod 444 ncov.$ncovTime.json
ln -sf ncov.$ncovTime.json ncov.json

#Generate bed and VCF files
$ottoDir/nextstrain.py

# bgzip & tabix the VCF files
bgzip -f nextstrainSamples.vcf
tabix -p vcf nextstrainSamples.vcf.gz
for clade in A1a A2 A2a A3 A6 A7 B B1 B2 B4; do
  bgzip -f nextstrainSamples$clade.vcf
  tabix -p vcf nextstrainSamples$clade.vcf.gz
done
bgzip -f nextstrainRecurrentBiallelic.vcf
tabix -p vcf nextstrainRecurrentBiallelic.vcf.gz

# bigBed-ify the gene names, "clades" and discarded/blacklisted/informative tracks for David
bedToBigBed -type=bed4 -tab -verbose=0 nextstrainGene.bed $chromSizes \
    nextstrainGene.bb

sort -k2n,2n nextstrainClade.bed > nextstrainClade.sorted.bed
bedToBigBed -as=$ottoDir/nextstrainClade.as -type=bed12+7 -tab -verbose=0 \
    nextstrainClade.sorted.bed $chromSizes \
    nextstrainClade.bb

bedToBigBed -type=bed4 -tab -verbose=0 nextstrainDiscarded.bed $chromSizes \
    nextstrainDiscarded.bb

bedToBigBed -type=bed4 -tab -verbose=0 nextstrainBlacklisted.bed $chromSizes \
    nextstrainBlacklisted.bb

bedToBigBed -type=bed4 -tab -verbose=0 nextstrainInformative.bed $chromSizes \
    nextstrainInformative.bb

# bigWig for the tree parsimony scores track for David
bedGraphToBigWig nextstrainParsimony.bedGraph $chromSizes nextstrainParsimony.bw

# Max's nextstrainSamples*.bedGraph allele count bigWigs:
for i in nextstrainSamples*.vcf.gz; do
    base=`basename $i .vcf.gz`
    zcat $i | cut -f1,2,8 | cut -d';' -f1 | grep -v '#' | sed -e 's/AC=//g' | cut -f1 -d, \
        | tawk '{print $1, $2, $2+1, $3}' > $base.bedGraph
    bedGraphToBigWig $base.bedGraph $chromSizes $base.bigWig
done

# Install
mkdir -p $ottoDir/current
cp -pf $runDir/nextstrainGene.bb $runDir/nextstrainClade.bb \
    $runDir/nextstrain*.vcf.gz{,.tbi} \
    $runDir/nextstrain*.nh \
    $runDir/nextstrainSamples*.bigWig \
    $ottoDir/current/
ln -sf $ottoDir/current/nextstrainGene.bb $ottoDir/current/nextstrainClade.bb \
    $ottoDir/current/nextstrain*.vcf.gz{,.tbi} \
    $ottoDir/current/nextstrain*.nh \
    $ottoDir/current/nextstrainSamples*.bigWig \
    $gbdbDir/

# Install but don't archive (for now) the experimental tracks for David.
cp -pf $runDir/nextstrain{Discarded,Blacklisted,Informative}.bb \
    $runDir/nextstrainParsimony.bw \
    $ottoDir/current/
ln -sf $ottoDir/current/nextstrain{Discarded,Blacklisted,Informative}.bb \
    $ottoDir/current/nextstrainParsimony.bw \
    $gbdbDir/

# Daily archive (may overwrite files from earlier today)
mkdir -p $ottoDir/archive/$today
cp -pf $runDir/nextstrainGene.bb $runDir/nextstrainClade.bb \
    $runDir/nextstrain*.vcf.gz{,.tbi} \
    $runDir/nextstrain*.nh \
    $runDir/nextstrainSamples*.bigWig \
    $runDir/ncov.json \
    $ottoDir/archive/$today

echo "Updated nextstrain/ncov `date` (ncov.json date $latestDate)"
