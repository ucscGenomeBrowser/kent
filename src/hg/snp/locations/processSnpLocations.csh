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
rm -f snp* $organism* bed.tab seq_contig.md liftAll.lft |& grep -v "No match."

echo Start command: processSnpLocations.csh $argv
echo File: ftp://ftp.ncbi.nlm.nih.gov/snp/$organism/genome_reports/$inputFile.gz
echo `date` Getting data
wget ftp://ftp.ncbi.nlm.nih.gov/snp/$organism/genome_reports/$inputFile.gz|& grep -v 0K|grep -v ^L|grep -v ^C|grep -v done|grep -v ^$

if ( "$organism" == "human" ) then
    ln -s ../../../../seq_contig.md .
    ln -s ../../../../jkStuff/liftAll.lft .

    echo `date` Unzipping data
    gunzip $inputFile.gz

    echo `date` Flipping contigs
    calcFlipSnpPos seq_contig.md $inputFile $inputFile.flipped

    echo `date` Making temp file snpMap.contig.bed
    awk -f ~/lib/snpMapFilter.awk $inputFile.flipped > snpMap.contig.bed
    gzip $inputFile.flipped

    echo `date` Lifting snpMap.contig.bed to snpMap.bed
    liftUp snpMap.bed liftAll.lft warn snpMap.contig.bed
    # rm snpMap.contig.bed

    echo `date` Making snpNih.bed
    grep BAC_OVERLAP snpMap.bed | awk -f ~/lib/snpFilter.awk >  snpNih.bed
    grep OTHER       snpMap.bed | awk -f ~/lib/snpFilter.awk >> snpNih.bed

    echo `date` Making snpTsc.bed
    grep RANDOM      snpMap.bed | awk -f ~/lib/snpFilter.awk >  snpTsc.bed
    grep MIXED       snpMap.bed | awk -f ~/lib/snpFilter.awk >> snpTsc.bed

    # snpTsc is specific to human, so load it here
    echo `date` Loading snpTsc.bed
    hgLoadBed $database snpTsc snpTsc.bed -tab
else
    # rat and mouse files come with chromosome coordinates, 
    # so they don't need seq_contig_md and liftAll.lft
    # chromosome names are changed (chrMT->chrM, chrUn is ignored)

    echo `date` Making snpNih.bed
    gunzip $inputFile | createMouseSnpNihBed > snpNih.bed

    echo `date` Making snpMap.bed
    gunzip $inputFile | createMouseSnpMapBed > snpMap.bed

endif

# load data into database
echo `date` Loading snpNih.bed
hgLoadBed $database snpNih snpNih.bed -tab

echo `date` Loading snpMap.bed
hgLoadBed $database snpMap snpMap.bed -sqlTable=$HOME/kent/src/hg/lib/snpMap.sql -tab

# useful data checks:

# wc -l snpTsc.bed; hg16 -e "select count(*) from snpTsc; select * from snpTsc limit 5; desc snpTsc; show indexes from snpTsc"
# wc -l snpNih.bed; hg16 -e "select count(*) from snpNih; select * from snpNih limit 5; desc snpNih; show indexes from snpNih"
# wc -l snpMap.bed; hg16 -e "select count(*) from snpMap; select * from snpMap limit 5; desc snpMap; show indexes from snpMap"

# wc -l snpNih.bed; mm4  -e "select count(*) from snpNih; select * from snpNih limit 5; desc snpNih; show indexes from snpNih"
# wc -l snpMap.bed; mm4  -e "select count(*) from snpMap; select * from snpMap limit 5; desc snpMap; show indexes from snpMap"

# wc -l snpNih.bed; rn3  -e "select count(*) from snpNih; select * from snpNih limit 5; desc snpNih; show indexes from snpNih"
# wc -l snpMap.bed; rn3  -e "select count(*) from snpMap; select * from snpMap limit 5; desc snpMap; show indexes from snpMap"

# clean up
echo `date` Zipping input files
# rm -f bed.tab
gzip $inputFile* snp*.bed
echo `date` Done.

