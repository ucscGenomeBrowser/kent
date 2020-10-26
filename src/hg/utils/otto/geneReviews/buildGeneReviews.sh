#!/bin/sh 
set -e
# processing raw data file from GENEREVIEWS

geneDiseaseFile="geneReviewsGeneDiseases.tab"

function createGeneReviewTables() { 
#Create geneReviews table and bigBed

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
rm -f geneReviews.$1.tab
cat grRefGene.lst | while read G
    do
    if [ "$1" = "hg38" ]
    then
        hgsql $1 -N -e \
            "SELECT e.chrom,e.txStart,e.txEnd,e.name2 \
            FROM gencodeAnnotV35 e where e.name2 ='${G}' \
            ORDER BY e.chrom,e.txStart;" > temp.in
    else
        hgsql $1 -N -e \
            "SELECT e.chrom,e.txStart,e.txEnd,j.geneSymbol \
            FROM knownGene e, kgXref j WHERE e.name = j.kgID AND \
            j.geneSymbol ='${G}' ORDER BY e.chrom,e.txStart;" > temp.in
    fi
    bedRemoveOverlap temp.in temp.out
    cat temp.out >> geneReviews.$1.tab
    done
rm temp.*

# load the collapsed bed4 file to database
hgsql $1 -e 'drop table if exists geneReviewsNew'

# NOTE: keeping table for backwards compatibility and search (for now)
hgLoadBed $1 geneReviewsNew geneReviews.$1.tab

# Create big bed file with diseases for mouseover
db=$1
perl ../geneRevsAddDiseases.pl $geneDiseaseFile geneReviews.$db.tab > geneReviewsExt.$db.tab
bedSort geneReviewsExt.$db.tab  stdout | uniq > geneReviewsExt.$db.bed
gbdb="/gbdb/$db/geneReviews"
mkdir -p $gbdb

# validate
oldLc=`bigBedToBed $gbdb/geneReviews.bb stdout | wc -l`
newLc=`wc -l < geneReviewsExt.$db.bed`
echo rowcount: old $oldLc new: $newLc
if [ $oldLc -ne 0 ]; then
    echo $oldLc $newLc | \
        awk '{if (($2-$1)/$1 > 0.1) {printf "validate $db GENE REVIEWS failed: old count: %d, new count: %d\n", $1,$2; exit 1;}}'
fi

#install
bedToBigBed -tab -type=bed9+2 -as=../geneReviews.as geneReviewsExt.$db.bed \
         /hive/data/genomes/$db/chrom.sizes geneReviews.$db.bb
rm -f $gbdb/geneReviews.bb
ln -s `pwd`/geneReviews.$db.bb $gbdb/geneReviews.bb

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

####### main ##########

# Create gene to disease file for extended BED mouseover
perl ../geneRevsListDiseases.pl NBKid_shortname_genesymbol.txt GRtitle_shortname_NBKid.txt > \
        $geneDiseaseFile

# Create tab files for internal geneReviewsGrshortNBKid and  geneReviewsGrshortTitleNBKid tables
cat NBKid_shortname_genesymbol.txt | grep -v "^#" \
    | awk  '{FS="\t"} {OFS="\t"} {if ($3!="Not applicable") print $3,$2,$1}' \
    | sort -k1 > geneReviewsGrshortNBKid.tab
grep -v "^#" GRtitle_shortname_NBKid.txt | sort -k1 > geneReviewsGrshortTitleNBKid.tab

# Generate a list of refSeq genes that have geneReview associated with it.
cat geneReviewsGrshortNBKid.tab | awk -F'\t' '{printf  "%s\n", $1}' \
    | sort  | uniq  > grRefGene.lst

createGeneReviewTables "hg38"
createGeneReviewTables "hg19"
createGeneReviewTables "hg18"

exit 0




