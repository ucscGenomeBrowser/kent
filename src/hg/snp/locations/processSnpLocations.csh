#!/bin/csh

# check usage
if ($# != 4) then
    echo "Usage:   processSnpLocations.csh DB   Organism NCBI_Build dbSnpBuild"
    echo "Example: processSnpLocations.csh hg16 human    34         119"
    exit
endif

# parse input
set database  = $1
set organism  = $2
set ncbiBuild = $3
set snpBuild  = $4
set oo        = /cluster/data/$database
set LOCDIR    = ${HOME}/kent/src/hg/snp/locations

if      ($organism == "human") then
    set tables    = "snpTsc snpNih snpMap"
    set inputFile = $organism.b$ncbiBuild.snp$snpBuild
else 
    set tables    = "snpNih snpMap"
    set inputFile = $organism.ncbi.b$ncbiBuild
endif


# Run from directory $oo/bed/snp/build$snpBuild/snpMap
mkdir -p $oo/bed/snp/build$snpBuild/snpMap
cd       $oo/bed/snp/build$snpBuild/snpMap
# remove existing files that might get in the way.
rm -f snp* $organism* bed.tab seq_contig.md liftAll.lft |& grep -v "No match."

echo Start command: processSnpLocations.csh $argv
echo File: \
	ftp://ftp.ncbi.nlm.nih.gov/snp/$organism/genome_reports/$inputFile.gz
echo `date` Getting data
wget ftp://ftp.ncbi.nlm.nih.gov/snp/$organism/genome_reports/$inputFile.gz \
	    |& grep -v 0K|grep -v ^L|grep -v ^C|grep -v done|grep -v ^$

if ( "$organism" == "human" ) then
    ln -s ../../../../seq_contig.md .
    ln -s ../../../../jkStuff/liftAll.lft .

    echo `date` Unzipping data
    gunzip $inputFile.gz

    echo `date` Flipping contigs
    calcFlipSnpPos seq_contig.md $inputFile $inputFile.flipped

    echo `date` Making temp file snpMap.contig.bed
    awk -f $LOCDIR/snpMapFilter.awk \
	$inputFile.flipped > snpMap.contig.bed
    rm -f $inputFile.flipped

    echo `date` Lifting snpMap.contig.bed to snpMap.bed
    liftUp snpMap.bed liftAll.lft warn snpMap.contig.bed
    rm -f snpMap.contig.bed

    echo `date` Making snpNih.bed
    grep BAC_OVERLAP snpMap.bed | awk -f $LOCDIR/snpFilter.awk >  snpNih.bed
    grep OTHER       snpMap.bed | awk -f $LOCDIR/snpFilter.awk >> snpNih.bed

    echo `date` Making snpTsc.bed
    grep RANDOM      snpMap.bed | awk -f $LOCDIR/snpFilter.awk >  snpTsc.bed
    grep MIXED       snpMap.bed | awk -f $LOCDIR/snpFilter.awk >> snpTsc.bed

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
hgLoadBed $database snpMap snpMap.bed \
    -sqlTable=$HOME/kent/src/hg/lib/snpMap.sql -tab

# useful data checks:
foreach table in ( $tables )
    wc -l $table.bed; 
    echo "select count(*) from $table"  | hgsql $database
    echo "select * from $table limit 5" | hgsql $database
    echo "desc $table"                  | hgsql $database
    echo "show indexes from $table"     | hgsql $database
end

# clean up
echo `date` Zipping input files
rm -f bed.tab
gzip $inputFile* snp*.bed
echo `date` Done.

