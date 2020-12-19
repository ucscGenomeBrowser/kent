#!/bin/sh 
set -e
# processing raw data file from GENEREVIEWS

geneDiseaseFile="geneReviewsGeneDiseases.tab"

function createGeneReviewBb() { 

echo "Creating geneReviews.$1.bb"

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
}

####### main ##########

# Create gene to disease file for extended BED mouseover
perl ../geneRevsListDiseases.pl NBKid_shortname_genesymbol.txt GRtitle_shortname_NBKid.txt > \
        $geneDiseaseFile

# Create tab files for internal geneReviewsGrshortNBKid and geneReviewsGrshortTitleNBKid tables
cat NBKid_shortname_genesymbol.txt | grep -v "^#" \
    | awk  '{FS="\t"} {OFS="\t"} {if ($3!="Not applicable") print $3,$2,$1}' \
    | sort -k1 > geneReviewsGrshortNBKid.tab
grep -v "^#" GRtitle_shortname_NBKid.txt | sort -k1 > geneReviewsGrshortTitleNBKid.tab

# Generate a list of refSeq genes that have geneReview associated with it.
cat geneReviewsGrshortNBKid.tab | awk -F'\t' '{printf  "%s\n", $1}' \
    | sort  | uniq  > grRefGene.lst

createGeneReviewBb "hg38"
createGeneReviewBb "hg19"
createGeneReviewBb "hg18"

exit 0




