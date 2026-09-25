#!/bin/bash
# Build the SFARI SPARK WGS 45k sites VCF from the release's AF table.
# Usage: sparkWgs45kToVcf.sh <pvcf_variant_frequencies.tsv> <out.vcf.gz> [threads]
# Splits the table per chromosome, converts each with sparkWgs45kToVcf.py
# (AN estimate, AC=round(AF*AN), singletons dropped), left-aligns with
# bcftools norm, sorts and concatenates into one bgzipped, tabix-indexed VCF.
set -euo pipefail

inTsv=$1
outVcf=$2
threads=${3:-24}
scriptDir=$(dirname "$(readlink -f "$0")")
refFa=/hive/data/genomes/hg38/bed/varFreqs/all/hg38.fa
work=$(dirname "$outVcf")/work
mkdir -p "$work"

# 1. split by chromosome in one pass (input is already sorted by chrom)
awk -F'\t' -v d="$work" 'NR>1 {if ($1!=c) {if (f) close(f); c=$1; f=d"/"c".tsv"} print > f}' "$inTsv"

# 2. convert + left-align/trim + sort, per chromosome in parallel
python3 "$scriptDir/sparkWgs45kToVcf.py" -H /dev/null > "$work/header.vcf" 2>/dev/null
convertOne() {
    c=$1; work=$2; scriptDir=$3; refFa=$4
    (cat "$work/header.vcf"; python3 "$scriptDir/sparkWgs45kToVcf.py" "$work/$c.tsv" 2> "$work/$c.convert.log") \
      | bcftools norm --check-ref w -f "$refFa" -Ou 2> "$work/$c.norm.log" \
      | bcftools sort -T "$work/sort.$c" -m 4G -Ob -o "$work/$c.bcf"
    bcftools index "$work/$c.bcf"
}
export -f convertOne
ls "$work"/chr*.tsv | xargs -n1 basename | sed 's/.tsv$//' \
    | parallel -j "$threads" convertOne {} "$work" "$scriptDir" "$refFa"

# 3. concatenate in chrom.sizes order
chroms=$(cut -f1 /hive/data/genomes/hg38/chrom.sizes | grep -v _ | sort -V)
files=""
for c in $chroms; do [ -s "$work/$c.bcf" ] && files="$files $work/$c.bcf"; done
bcftools concat --threads 8 -Oz -o "$outVcf" $files
tabix -p vcf "$outVcf"

cat "$work"/chr*.convert.log
grep -h 'Lines' "$work"/chr*.norm.log || true
