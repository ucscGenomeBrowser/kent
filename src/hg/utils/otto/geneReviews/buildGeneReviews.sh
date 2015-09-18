#!/bin/sh 
set -e
# processing raw data file from GENEREVIEWS

#function to create geneReviews table
function createGeneReviewTables() { 
# Load the internal working table geneReviewsGrshortNBKid to hg38/hg19/hg18
hgsql $1 -e 'drop table if exists geneReviewsGrshortNBKidNew'
hgsql $1 -e 'create table geneReviewsGrshortNBKidNew select * from geneReviewsGrshortNBKid limit 0'
hgsql $1 -e \
'load data local infile "geneReviewsGrshortNBKid.tab" into table geneReviewsGrshortNBKidNew'

# Load the internal working table geneReviewsGrshortTitleNBKid to
# hg38/19/18
hgsql $1 -e 'drop table if exists geneReviewsGrshortTitleNBKidNew'
hgsql $1 -e 'create table geneReviewsGrshortTitleNBKidNew select * from geneReviewsGrshortTitleNBKid limit 0'
hgsql $1 -e \
'load data local infile "geneReviewsGrshortTitleNBKid.tab" into table geneReviewsGrshortTitleNBKidNew'

# for each refGen in grRefGene.lst, create a non-overlapping bed row. 
if [ -e "geneReviews.tab" ]
then
    rm geneReviews.tab
fi
cat grRefGene.lst | while read G
    do
    hgsql $1 -N -e \
        "SELECT e.chrom,e.txStart,e.txEnd,j.geneSymbol \
        FROM knownGene e, kgXref j WHERE e.name = j.kgID AND \
        j.geneSymbol ='${G}' ORDER BY e.chrom,e.txStart;" > temp.in
        bedRemoveOverlap temp.in temp.out
        cat temp.out >> geneReviews.tab
    done
rm temp.*

# load the collapsed bed4 file to database
hgsql $1 -e 'drop table if exists geneReviewsNew'
hgLoadBed $1 geneReviewsNew geneReviews.tab

# Create and load geneReviewsDetail table
hgsql $1 -N -e \
    "SELECT  s.geneSymbol, s.grShort, t.NBKid, t.grTitle \
        FROM geneReviewsGrshortNBKidNew s, geneReviewsGrshortTitleNBKidNew t \
        WHERE s.grShort = t.grShort ORDER BY s.geneSymbol;" > geneReviewsDetail.tab

hgsql $1 -e 'drop table if exists geneReviewsDetailNew'
hgsql $1 -e 'create table geneReviewsDetailNew select * from geneReviewsDetail limit 0'
hgsql $1 -e \
'load data local infile "geneReviewsDetail.tab" into table geneReviewsDetailNew'
}

# Create tab files for internal geneReviewsGrshortNBKid and
# geneReviewsGrshortTitleNBKid tables
cat NBKid_shortname_genesymbol.txt | grep -v "^#" \
    | awk  '{FS="\t"} {OFS="\t"} {if ($3!="Not applicable") print $3,$2,$1}' \
    | sort -k1 > geneReviewsGrshortNBKid.tab
grep -v "^#" GRtitle_shortname_NBKid.txt | sort -k1 > geneReviewsGrshortTitleNBKid.tab

# Generate a list of refSeq genes that have geneReview assoicate with
# it.
cat geneReviewsGrshortNBKid.tab | awk -F'\t' '{printf  "%s\n", $1}' \
    | sort  | uniq  > grRefGene.lst

createGeneReviewTables "hg38"
createGeneReviewTables "hg19"
createGeneReviewTables "hg18"

exit 0




