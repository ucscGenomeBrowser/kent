#!/bin/csh

# check usage
if ($# != 3) then
    echo "Usage:   processSnps.csh Database Organism dbSnpBuild"
    echo "Example: processSnps.csh hg16     human    121"
    exit
endif

# parse input
set database  = $1
set organism  = $2
set snpBuild  = $3
set details   = /cluster/data/$database/bed/snp/build$snpBuild/details
set fileBase  = dbSnpRS${database}Build$snpBuild
set nice      = "nice "

# Run from directory $details
mkdir -p $details
cd       $details
# remove existing files that might get in the way.
rm -rf snp* $organism* bed.tab seq_contig.md liftAll.lft Done Observed |& grep -v "No match."
mkdir -p $details/Done $details/Observed

echo `date` Working Directory: `pwd`
echo `date` Start command: processSnpDetails.csh $argv
echo `date` Files: ftp://ftp.ncbi.nlm.nih.gov/snp/${organism}/XML/ds_chXX.xml.gz

if ("$organism" == "human") then
    set chroms = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y"
else if ("$organism" == "mouse") then
    set chroms = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 X MT"
else if ("$organism" == "rat") then
    set chroms = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 X Y M"
else
    echo "Organism ($organism) not supported yet."
endif

touch $fileBase.obs
foreach chrom ( $chroms )
    echo
    echo `date` Getting data for chr$chrom
    $nice wget ftp://ftp.ncbi.nlm.nih.gov/snp/$organism/XML/ds_ch$chrom.xml.gz \
	|& grep ds_ch | grep -v "=" | grep -v XML
    
    echo `date` Getting details from ds_ch$chrom.xml
    $nice zcat ds_ch$chrom.xml | parseDbSnpXML > Observed/ch$chrom.obs

    echo `date` Collecting details
    $nice cat  Observed/ch$chrom.obs >> $fileBase.obs

    echo `date` Zipping ch$chrom.obs
    $nice gzip Observed/ch$chrom.obs
end

echo `date` Finished.
echo

exit;

# clean up
echo `date` Zipping files
$nice gzip $fileBase.err $fileBase.obs
echo `date` Done.
echo

echo load data local infile \"$fileBase.out\" into table $table | hgsql hgFixed

# useful data checks
echo "=================================="
echo $database $organism $table
echo wc -l $table.bed
echo select count\(\*\) from $table | hgsql hgFixed
echo
echo desc $table                    | hgsql hgFixed
echo
echo show indexes from $table       | hgsql hgFixed
echo
echo select \* from $table limit 5  | hgsql hgFixed
echo

echo `date` Finished: processSnpDetails.csh $argv
