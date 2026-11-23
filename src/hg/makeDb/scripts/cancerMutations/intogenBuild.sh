#!/bin/bash
# Download the IntOGen driver gene compendium and build the intogen bigBed for hg38,
# then lift it to hg19.
#   cd /hive/data/genomes/hg38/bed/cancerVariants/intogen
#   ~/kent/src/hg/makeDb/scripts/cancerMutations/intogenBuild.sh
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
release=20240920
gff=/hive/data/genomes/hg38/bed/cancerVariants/cancerHotspots/orig/Homo_sapiens.GRCh38.115.chr.gff3.gz
hg19Dir=/hive/data/genomes/hg19/bed/cancerVariants/intogen

get() {
    url=$1; out=$2
    if [ -s "$out" ]; then echo "$out exists, skipping"; return; fi
    rm -f "$out.part"
    curl -sSfL -o "$out.part" "$url"
    mv "$out.part" "$out"
}

mkdir -p orig
for f in IntOGen-Drivers-$release.zip IntOGen-Cohorts-$release.zip; do
    get "https://www.intogen.org/download?file=$f" orig/$f
    unzip -oq orig/$f -d orig
done
# the zip files contain directories named after the internal release date
comp=$(ls orig/*_IntOGen-Drivers/Compendium_Cancer_Genes.tsv)
cohorts=$(ls orig/*_IntOGen-Cohorts/cohorts.tsv)

python3 $scriptDir/intogenToBed.py $comp $cohorts $gff intogen.unsorted.bed
sort -k1,1 -k2,2n intogen.unsorted.bed > intogen.bed
bedToBigBed -tab -type=bed12+14 -as=$scriptDir/intogen.as intogen.bed \
    /hive/data/genomes/hg38/chrom.sizes intogen.bb

# hg19: liftOver the gene models. Genes that do not lift (exon boundary changes, PAR) are
# taken from the same transcript in Ensembl GRCh37 release 87, if it exists there.
gff37=/hive/data/genomes/hg19/bed/cancerVariants/cancerHotspots/orig/Homo_sapiens.GRCh37.87.chr.gff3.gz
mkdir -p $hg19Dir
liftOver -tab -bedPlus=12 intogen.bed /gbdb/hg38/liftOver/hg38ToHg19.over.chain.gz \
    $hg19Dir/intogen.lifted.bed $hg19Dir/intogen.unmapped
python3 $scriptDir/intogenToBed.py $comp $cohorts $gff37 $hg19Dir/intogen.grch37.bed
grep -v '^#' $hg19Dir/intogen.unmapped | cut -f4 > $hg19Dir/unmappedGenes.txt
awk -F'\t' 'NR==FNR {g[$1]; next} $4 in g' $hg19Dir/unmappedGenes.txt $hg19Dir/intogen.grch37.bed \
    | cat $hg19Dir/intogen.lifted.bed - | sort -k1,1 -k2,2n > $hg19Dir/intogen.bed
bedToBigBed -tab -type=bed12+14 -as=$scriptDir/intogen.as $hg19Dir/intogen.bed \
    /hive/data/genomes/hg19/chrom.sizes $hg19Dir/intogen.bb
echo "hg19: $(wc -l < $hg19Dir/intogen.bed) genes, not lifted: $(paste -sd" " $hg19Dir/unmappedGenes.txt)"
