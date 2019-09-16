#!/bin/bash

set -beEu -o pipefail

cd /hive/data/genomes/asmHubs/VGP/ucscNames

ls -d */bbi | sed -e 's#/bbi##;' | while read asmId
do
   mkdir -p /gbdb/hubs/VGP/genomes/$asmId /gbdb/hubs/VGP/genomes/$asmId/html
   rm -f /gbdb/hubs/VGP/genomes/$asmId/bbi
   rm -f /gbdb/hubs/VGP/genomes/$asmId/ixIxx
   rm -f /gbdb/hubs/VGP/genomes/$asmId/html/*.html
   rm -f /gbdb/hubs/VGP/genomes/$asmId/html/*.jpg
   rm -f /gbdb/hubs/VGP/genomes/$asmId/html/*.png
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.2bit
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.agp
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.agp.gz
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.genomes.txt
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.trackDb.txt
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.chrom.sizes
   rm -f /gbdb/hubs/VGP/genomes/$asmId/${asmId}_assembly_report.txt
   ln -s `pwd -P`/$asmId/bbi /gbdb/hubs/VGP/genomes/$asmId/bbi
   ln -s `pwd -P`/$asmId/ixIxx /gbdb/hubs/VGP/genomes/$asmId/ixIxx
   ln -s `pwd -P`/$asmId/html/*.html /gbdb/hubs/VGP/genomes/$asmId/html/
   ln -s `pwd -P`/$asmId/html/*.jpg /gbdb/hubs/VGP/genomes/$asmId/html/
   ln -s `pwd -P`/$asmId/trackData/addMask/$asmId.masked.2bit /gbdb/hubs/VGP/genomes/$asmId/$asmId.2bit
   ln -s `pwd -P`/$asmId/$asmId.agp.gz /gbdb/hubs/VGP/genomes/$asmId/$asmId.agp.gz
   ln -s `pwd -P`/$asmId/$asmId.genomes.txt /gbdb/hubs/VGP/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.chrom.sizes /gbdb/hubs/VGP/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.trackDb.txt /gbdb/hubs/VGP/genomes/$asmId
   ln -s `pwd -P`/$asmId/download/${asmId}_assembly_report.txt /gbdb/hubs/VGP/genomes/$asmId/
done
