#!/bin/csh

# check usage
if ($# != 4) then
    echo "Usage:   processSnpLocations.csh Database Organism NCBI_Build dbSnpBuild"
    echo "Example: processSnpLocations.csh hg16     human    34         119"
    exit
endif

# parse input
set database  = $1
set organism  = $2
set ncbiBuild = $3
set snpBuild  = $4
set oo        = /cluster/data/$database

if      ($organism == "human") then
    set inputFile = $organism.b$ncbiBuild.snp$snpBuild
else 
    set inputFile = $organism.ncbi.b$ncbiBuild
endif


# Run from directory $oo/bed/snp/build$snpBuild/snpMap
mkdir -p $oo/bed/snp/build$snpBuild/snpMap
cd       $oo/bed/snp/build$snpBuild/snpMap
# remove existing files that might get in the way.
rm -f log snpNih* snpTsc* snpMap* $inputFile* | & grep -v "No match."

echo Start command: processSnpLocations.csh $argv

echo `date` Getting data
wget ftp://ftp.ncbi.nlm.nih.gov/snp/$organism/genome_reports/$inputFile.gz |& grep -v 0K|grep -v ^L|grep -v ^C | grep -v done

echo `date` Unzipping data
gunzip $inputFile.gz

if ( "$organism" == "human" ) then
    ln -s ../../../../seq_contig.md .
    ln -s ../../../../jkStuff/liftAll.lft .

    echo `date` Flipping contigs
    calcFlipSnpPos seq_contig.md $inputFile $inputFile.flipped

    echo `date` Making temp file snpMap.contig.bed
    awk -f ~/lib/snpMapFilter.awk $inputFile.flipped > snpMap.contig.bed

    echo `date` Lifting snpMap.contig.bed to snpMap.bed
    liftUp snpMap.bed liftAll.lft warn snpMap.contig.bed

    echo `date` Making snpNih.bed
    grep BAC_OVERLAP snpMap.bed | awk -f ~/lib/snpFilter.awk >  snpNih.contig.bed
    grep OTHER       snpMap.bed | awk -f ~/lib/snpFilter.awk >> snpNih.contig.bed

    echo `date` Making snpTsc.bed
    grep RANDOM      snpMap.bed | awk -f ~/lib/snpFilter.awk >  snpTsc.contig.bed
    grep MIXED       snpMap.bed | awk -f ~/lib/snpFilter.awk >> snpTsc.contig.bed

    # snpTsc is specific to human, so load it here
    echo `date` Loading snpTsc.bed
    hgLoadBed $database snpTsc snpTsc.bed -tab
else
    # rat and mouse files come with chromosome coordinates, 
    # so they don't need seq_contig_md and liftAll.lft
    # chromosome names are changed (chrMT->chrM, chrUn is ignored)

    echo `date` Making snpNih.bed
    createMouseSnpNihBed < $inputFile > snpNih.bed

    echo `date` Making snpMap.bed
    createMouseSnpMapBed < $inputFile > snpMap.bed
endif

# load data into database
echo `date` Loading snpNih.bed
hgLoadBed $database snpNih snpNih.bed -tab

echo `date` Loading snpMap.bed
hgLoadBed $database snpMap snpMap.bed -sqlTable=$HOME/src/hg/lib/snpMap.sql -tab

# clean up
echo `date` Zipping input files
gzip $inputFile* snp*.bed
rm -f bed.tab
echo `date` Done.

echo
echo $database " -e \"desc snpMap; desc snpNih;"
echo $database " -e \"show indexes from  snpNih; show indexes from snpMap\""
echo $database " -e \"select count(*) from snpMap limit 10; select count(*) from snpNih limit 10;\""
echo $database " -e \"select * from snpMap limit 10; select * from snpNih limit 10;\""
