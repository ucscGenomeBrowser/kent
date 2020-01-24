#!/bin/bash

set -beEu -o pipefail

export Name="GlobalReference"
export subDir="globalReference"

cd /hive/data/genomes/asmHubs/${subDir}

ls -d */bbi | sed -e 's#/bbi##;' | while read asmId
do
   mkdir -p /gbdb/hubs/$subDir/genomes/$asmId /gbdb/hubs/$subDir/genomes/$asmId/html
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/bbi
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/ixIxx
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/html/*.html
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/$asmId.2bit
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/$asmId.agp
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/$asmId.agp.gz
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/$asmId.genomes.txt
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/$asmId.trackDb.txt
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/$asmId.chrom.sizes
   rm -f /gbdb/hubs/$subDir/genomes/$asmId/${asmId}_assembly_report.txt
   rm -f /gbdb/hubs/$subDir/hub.txt
   rm -f /gbdb/hubs/$subDir/groups.txt
   rm -f /gbdb/hubs/$subDir/index.html
   rm -f /gbdb/hubs/$subDir/testIndex.html
   rm -f /gbdb/hubs/$subDir/asmStats$Name.html
   rm -f /gbdb/hubs/$subDir/testAsmStats$Name.html
   rm -f /gbdb/hubs/$subDir/genomes.txt
   ln -s `pwd -P`/$asmId/bbi /gbdb/hubs/$subDir/genomes/$asmId/bbi
   ln -s `pwd -P`/$asmId/ixIxx /gbdb/hubs/$subDir/genomes/$asmId/ixIxx
   ln -s `pwd -P`/$asmId/html/*.html /gbdb/hubs/$subDir/genomes/$asmId/html/
   ln -s `pwd -P`/$asmId/trackData/addMask/$asmId.masked.2bit /gbdb/hubs/$subDir/genomes/$asmId/$asmId.2bit
   ln -s `pwd -P`/$asmId/$asmId.agp.gz /gbdb/hubs/$subDir/genomes/$asmId/$asmId.agp.gz
   ln -s `pwd -P`/$asmId/$asmId.genomes.txt /gbdb/hubs/$subDir/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.chrom.sizes /gbdb/hubs/$subDir/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.trackDb.txt /gbdb/hubs/$subDir/genomes/$asmId
   ln -s `pwd -P`/$asmId/download/${asmId}_assembly_report.txt /gbdb/hubs/$subDir/genomes/$asmId/
   ln -s /hive/data/genomes/asmHubs/${subDir}/hub.txt /gbdb/hubs/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/groups.txt /gbdb/hubs/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/index.html /gbdb/hubs/$subDir/
   sed -e "s/hgdownload.soe/genome-test.gi/g; s/asmStats$Name/testAsmStats$Name/;" /hive/data/genomes/asmHubs/${subDir}/index.html > /hive/data/genomes/asmHubs/${subDir}/testIndex.html
   chmod +x /hive/data/genomes/asmHubs/${subDir}/testIndex.html
   ln -s /hive/data/genomes/asmHubs/${subDir}/testIndex.html /gbdb/hubs/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/genomes.txt /gbdb/hubs/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/asmStats$Name.html /gbdb/hubs/$subDir/
   sed -e 's/hgdownload.soe/genome-test.gi/g; s/index.html/testIndex.html/;' /hive/data/genomes/asmHubs/${subDir}/asmStats$Name.html > /hive/data/genomes/asmHubs/${subDir}/testAsmStats$Name.html
   chmod +x /hive/data/genomes/asmHubs/${subDir}/testAsmStats$Name.html
   ln -s /hive/data/genomes/asmHubs/${subDir}/testAsmStats$Name.html /gbdb/hubs/$subDir/
done
