#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/nextstrainNcov/doUpdate.sh

ottoDir=/hive/data/outside/otto/nextstrainNcov
chromSizes=/hive/data/genomes/wuhCor1/chrom.sizes
gbdbDir=/gbdb/wuhCor1/nextstrain

ncovJsonUrl='https://nextstrain.org/charon/getDataset?prefix=/ncov/open/global/6m'

cd $ottoDir

rm -f ncov.json
# HT Thomas Sibley: use --compressed flag to send "Accept-Encoding: gzip" and auto-decompress:
curl -sS --compressed "$ncovJsonUrl" > ncov.json
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
mv -f ncov.json old.ncov.json
mv ncov.json.date old.ncov.json.date

runDir=$ottoDir/$today
cd $runDir
chmod 444 ncov.$ncovTime.json
ln -sf ncov.$ncovTime.json ncov.json

# remove older result files in case a clade went away, as A2 did in ncov.2020-05-27-08:06.json
rm -f *.vcf.gz* *.bedGraph *.bigWig *.nh

#Generate bed, VCF etc. files
$ottoDir/nextstrain.py

# bgzip & tabix the VCF files
for f in nextstrain*.vcf; do
  bgzip -f $f
  tabix -p vcf $f.gz
done

# bigBed-ify the gene names, "clades" and discarded/blacklisted/informative tracks for David
bedToBigBed -type=bed4 -tab -verbose=0 nextstrainGene.bed $chromSizes \
    nextstrainGene.bb

sort -k2n,2n nextstrainClade.bed > nextstrainClade.sorted.bed
bedToBigBed -as=$ottoDir/nextstrainClade.as -type=bed12+7 -tab -verbose=0 \
    nextstrainClade.sorted.bed $chromSizes \
    nextstrainClade.bb

if [ -f nextstrainOldClade.bed ]; then
    sort -k2n,2n nextstrainOldClade.bed > nextstrainOldClade.sorted.bed
    bedToBigBed -as=$ottoDir/nextstrainClade.as -type=bed12+7 -tab -verbose=0 \
        nextstrainOldClade.sorted.bed $chromSizes \
        nextstrainOldClade.bb
fi

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
    zcat $i \
    | grep -v '#' \
    | perl -wne '@w=split("\t");
                 $w[7] =~ m/AC=(\d+)[\d,]*;AN=(\d+)/ ||
                          die "Cant find AC and AN in |$w[7]|";
                 print join("\t", $w[0], $w[1]-1, $w[1], (sprintf "%.06f", $1 / $2)) . "\n";' \
      > $base.bedGraph
    if [ -s $base.bedGraph ]; then
        bedGraphToBigWig $base.bedGraph $chromSizes $base.bigWig
    fi
done

# Install public track files
mkdir $ottoDir/install
cp -pf $runDir/nextstrainGene.bb $runDir/nextstrain*Clade.bb \
    $runDir/nextstrain*.vcf.gz{,.tbi} \
    $runDir/nextstrain*.nh \
    $runDir/nextstrainSamples*.bigWig \
    $ottoDir/install/
rm -rf $ottoDir/current.bak
mv -f $ottoDir/current $ottoDir/current.bak
mv $ottoDir/install $ottoDir/current
rm -r $gbdbDir
mkdir $gbdbDir
ln -sf $ottoDir/current/nextstrainGene.bb $ottoDir/current/nextstrain*Clade.bb \
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
rm -f $ottoDir/archive/$today/*
cp -pf $runDir/nextstrainGene.bb $runDir/nextstrain*Clade.bb \
    $runDir/nextstrain*.vcf.gz{,.tbi} \
    $runDir/nextstrain*.nh \
    $runDir/nextstrainSamples*.bigWig \
    $ottoDir/archive/$today

echo "Updated nextstrain/ncov `date` (ncov.json dated $ncovTime)"
