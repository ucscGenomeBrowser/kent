#!/bin/bash
# Don't exit on error, just attempt to run pangolin on ncbi.latest

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/pangolinNcbi.sh

ottoDir=/hive/data/outside/otto/sarscov2phylo
ncbiDir=$ottoDir/ncbi.latest

cd $ncbiDir

source ~/.bashrc
conda activate pangolin
pangolin <(xzcat genbank.fa.xz) >& pangolin.log

exit 0
