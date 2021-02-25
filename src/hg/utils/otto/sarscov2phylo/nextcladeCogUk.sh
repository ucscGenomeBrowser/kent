#!/bin/bash
# Don't exit on error, just attempt to run nextclade on cogUk.latest

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/nextcladeCogUk.sh

source ~/.bashrc

ottoDir=/hive/data/outside/otto/sarscov2phylo
cogUkDir=$ottoDir/cogUk.latest

cd $cogUkDir

# Nextclade needs input to be split into reasonably sized chunks (as of Jan. 2021).
splitDir=splitForNextclade
rm -rf $splitDir
mkdir $splitDir
faSplit about <(xzcat cog_all.fasta.xz) 30000000 $splitDir/chunk

rm -f nextclade.log nextclade.tsv
for chunkFa in $splitDir/chunk*.fa; do
    nextclade -j 50 -i $chunkFa -t >(cut -f 1,2 | tail -n+2 >> nextclade.tsv) >& nextclade.log
done

rm -rf $splitDir
exit 0
