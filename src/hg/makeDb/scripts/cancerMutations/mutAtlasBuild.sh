#!/bin/bash
# Build the Cancer Mutation Atlas bigBed (used by both mutAtlas and mutAtlasLolly).
# Run in the data directory after mutAtlasDownload.sh, e.g.
#   cd /hive/data/genomes/hg38/bed/cancerVariants/mutAtlas
#   ~/kent/src/hg/makeDb/scripts/cancerMutations/mutAtlasBuild.sh
set -beEu -o pipefail

scriptDir=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/hg38/chrom.sizes
gff=Homo_sapiens.GRCh38.115.chr.gff3.gz

# sites-only VCF, ID = line number in atlas.tsv
python3 $scriptDir/mutAtlasToVcf.py atlas.tsv $sizes | bcftools sort -Oz -o atlas.vcf.gz
bcftools index -f -t atlas.vcf.gz

# bcftools csq needs an indexed fasta
if [ ! -s hg38.fa.fai ]; then
    twoBitToFa /hive/data/genomes/hg38/hg38.2bit hg38.fa
    samtools faidx hg38.fa
fi
bcftools csq -l --force -n 64 --unify-chr-names chr,-,chr -f hg38.fa -g $gff atlas.vcf.gz -Ou 2> csq.log \
    | bcftools query -f '%ID\t%INFO/BCSQ\n' | sort -k1,1n -S4G > csq.tsv

# the transcripts that Ensembl marks as canonical (= MANE Select, where there is one)
zcat $gff | awk -F'\t' '$9 ~ /Ensembl_canonical/ && $9 ~ /ID=transcript:/' \
    | grep -o 'ID=transcript:ENST[0-9]*' | cut -d: -f2 | sort -u > canonicalTx.txt

python3 $scriptDir/mutAtlasToBed.py atlas.tsv csq.tsv canonicalTx.txt oncotree.json mutAtlasFilters.ra \
    | sort -k1,1 -k2,2n -S4G > mutAtlas.bed
bedToBigBed -tab -type=bed9+25 -as=$scriptDir/mutAtlas.as mutAtlas.bed $sizes mutAtlas.bb
rm -f hg38.fa hg38.fa.fai
