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
set tables    = "snpMap"

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

else
    # rat and mouse files come with chromosome coordinates, 
    # so they don't need seq_contig_md and liftAll.lft
    # chromosome names are changed (chrMT->chrM, chrUn is ignored)

    echo `date` Making snpMap.bed
    $LOCDIR/createMouseSnpMapBed < $inputFile > snpMap.bed

endif

echo `date` Loading snpMap.bed
hgLoadBed $database snpMapTemp snpMap.bed \
    -sqlTable=$HOME/kent/src/hg/lib/snpMapTemp.sql

echo insert into snpMapTemp select bin, chrom, chromStart, chromEnd, name, \
	\'Affy10K\' as source,  \'SNP\' as type from affy10K  | hgsql $database
echo insert into snpMapTemp select bin, chrom, chromStart, chromEnd, name, \
	\'Affy120K\' as source, \'SNP\' as type from affy120K | hgsql $database
echo delete from snpMap | hgsql $database
echo insert into snpMap select \* from snpMapTemp order by chrom, chromStart \
	| hgsql $database

# useful data checks:
foreach table ( $tables )
    echo "=================================="
    echo $database $organism $table
    echo wc -l $table.bed
    echo select count\(\*\) from $table | hgsql $database
    echo
    echo desc $table                    | hgsql $database
    echo
    echo show indexes from $table       | hgsql $database
    echo
    echo select \* from $table limit 5  | hgsql $database
    echo
end

# clean up
echo `date` Zipping input files
gzip $inputFile* snp*.bed
rm -f bed.tab
echo `date` Done.
