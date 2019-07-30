#!/bin/bash

ls -d */bbi | sed -e 's#/bbi##;' | while read asmId
do
   mkdir -p /gbdb/hubs/VGP/genomes/$asmId
   rm -f /gbdb/hubs/VGP/genomes/$asmId/bbi
   rm -f /gbdb/hubs/VGP/genomes/$asmId/ixIxx
   rm -f /gbdb/hubs/VGP/genomes/$asmId/html/*.html
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.2bit
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.genomes.txt
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.trackDb.txt
   rm -f /gbdb/hubs/VGP/genomes/$asmId/$asmId.chrom.sizes
   ln -s `pwd -P`/$asmId/bbi -f /gbdb/hubs/VGP/genomes/$asmId/bbi
   ln -s `pwd -P`/$asmId/ixIxx -f /gbdb/hubs/VGP/genomes/$asmId/ixIxx
   mkdir -p /gbdb/hubs/VGP/genomes/$asmId/html
   ln -s `pwd -P`/$asmId/html/*.html /gbdb/hubs/VGP/genomes/$asmId/html/
   ln -s `pwd -P`/$asmId/trackData/addMask/$asmId.masked.2bit /gbdb/hubs/VGP/genomes/$asmId/$asmId.2bit
   ln -s `pwd -P`/$asmId/$asmId.genomes.txt /gbdb/hubs/VGP/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.chrom.sizes /gbdb/hubs/VGP/genomes/$asmId
   ln -s `pwd -P`/$asmId/$asmId.trackDb.txt /gbdb/hubs/VGP/genomes/$asmId
done
