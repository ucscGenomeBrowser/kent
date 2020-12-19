#!/bin/bash

set -beEu -o pipefail

export Name="GlobalReference"
export subDir="globalReference"
export destDir="/usr/local/apache/htdocs-hgdownload/hubs"

cd /hive/data/genomes/asmHubs/${subDir}

ls -d */bbi | sed -e 's#/bbi##;' | while read asmId
do
   mkdir -p ${destDir}/$subDir/genomes/$asmId ${destDir}/$subDir/genomes/$asmId/html
   rm -f ${destDir}/$subDir/genomes/$asmId/bbi
   rm -f ${destDir}/$subDir/genomes/$asmId/ixIxx
   rm -f ${destDir}/$subDir/genomes/$asmId/html/*.html
   rm -f ${destDir}/$subDir/genomes/$asmId/$asmId.2bit
   rm -f ${destDir}/$subDir/genomes/$asmId/$asmId.agp
   rm -f ${destDir}/$subDir/genomes/$asmId/$asmId.agp.gz
   rm -f ${destDir}/$subDir/genomes/$asmId/$asmId.genomes.txt
   rm -f ${destDir}/$subDir/genomes/$asmId/$asmId.trackDb.txt
   rm -f ${destDir}/$subDir/genomes/$asmId/$asmId.chrom.sizes
   rm -f ${destDir}/$subDir/genomes/$asmId/${asmId}_assembly_report.txt
   rm -f ${destDir}/$subDir/hub.txt
   rm -f ${destDir}/$subDir/groups.txt
   rm -f ${destDir}/$subDir/index.html
   rm -f ${destDir}/$subDir/testIndex.html
   rm -f ${destDir}/$subDir/asmStats$Name.html
   rm -f ${destDir}/$subDir/testAsmStats$Name.html
   rm -f ${destDir}/$subDir/genomes.txt
   ln -s `pwd -P`/$asmId/bbi ${destDir}/$subDir/genomes/$asmId/bbi
   ln -s `pwd -P`/$asmId/ixIxx ${destDir}/$subDir/genomes/$asmId/ixIxx
   ln -s `pwd -P`/$asmId/html/*.html ${destDir}/$subDir/genomes/$asmId/html/
   ln -s `pwd -P`/$asmId/trackData/addMask/$asmId.masked.2bit ${destDir}/$subDir/genomes/$asmId/$asmId.2bit
   ln -s `pwd -P`/$asmId/$asmId.agp.gz ${destDir}/$subDir/genomes/$asmId/$asmId.agp.gz
   ln -s `pwd -P`/$asmId/$asmId.genomes.txt ${destDir}/$subDir/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.chrom.sizes ${destDir}/$subDir/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.trackDb.txt ${destDir}/$subDir/genomes/$asmId
   ln -s `pwd -P`/$asmId/download/${asmId}_assembly_report.txt ${destDir}/$subDir/genomes/$asmId/
   ln -s /hive/data/genomes/asmHubs/${subDir}/hub.txt ${destDir}/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/groups.txt ${destDir}/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/index.html ${destDir}/$subDir/
   sed -e "s/hgdownload.soe/genome-test.gi/g; s/asmStats$Name/testAsmStats$Name/;" /hive/data/genomes/asmHubs/${subDir}/index.html > /hive/data/genomes/asmHubs/${subDir}/testIndex.html
   chmod +x /hive/data/genomes/asmHubs/${subDir}/testIndex.html
   ln -s /hive/data/genomes/asmHubs/${subDir}/testIndex.html ${destDir}/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/genomes.txt ${destDir}/$subDir/
   ln -s /hive/data/genomes/asmHubs/${subDir}/asmStats$Name.html ${destDir}/$subDir/
   sed -e 's/hgdownload.soe/genome-test.gi/g; s/index.html/testIndex.html/;' /hive/data/genomes/asmHubs/${subDir}/asmStats$Name.html > /hive/data/genomes/asmHubs/${subDir}/testAsmStats$Name.html
   chmod +x /hive/data/genomes/asmHubs/${subDir}/testAsmStats$Name.html
   ln -s /hive/data/genomes/asmHubs/${subDir}/testAsmStats$Name.html ${destDir}/$subDir/
done
