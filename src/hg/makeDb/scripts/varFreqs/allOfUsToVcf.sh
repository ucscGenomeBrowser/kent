#!/bin/bash
# Concatenate the per-chromosome All of Us local-ancestry allele frequency
# VCFs from the Ioannidis lab (allele_freq_chr{1..22}.joined.vcf.gz) into one
# bgzipped, tabix-indexed VCF for the varFreqs "allofus" subtrack.
#
# The lab's files have chr-prefixed ##contig header lines but bare chromosome
# names ("1".."22") in the data lines. bcftools annotate --rename-chrs refuses
# that mismatch, so the data lines get their "chr" prefix with awk.
#
# Missing values: the lab applies the All of Us "rule of 20". When AC_<pop> or
# AN_<pop> was below 20, both are written as "." and R20_<pop>=0; AF_<pop> is
# kept. When an ancestry was not observed at a variant, its AC/AN/AF fields are
# absent and R20_<pop>=".". The records are passed through unchanged: "." is
# the standard VCF missing value and the browser and vcfToBigBed.py treat it as
# empty. The script prints a count of each of these three cases per population.
#
# usage: allOfUsToVcf.sh <inDir> <out.vcf.gz>

set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <inDir with allele_freq_chrN.joined.vcf.gz> <out.vcf.gz>" >&2
    exit 1
fi

inDir=$1
out=$2

inFiles=()
for i in $(seq 1 22); do
    f=$inDir/allele_freq_chr$i.joined.vcf.gz
    if [ ! -s "$f" ]; then
        echo "missing input file $f" >&2
        exit 1
    fi
    inFiles+=("$f")
done

bcftools concat --threads 8 -Ov "${inFiles[@]}" \
    | awk 'BEGIN{OFS=FS="\t"} /^#/{print; next} $1 !~ /^chr/ {$1="chr"$1} {print}' \
    | bgzip -@8 -c > "$out.tmp"
tabix -p vcf "$out.tmp"
mv "$out.tmp" "$out"
mv "$out.tmp.tbi" "$out.tbi"

# Summary: records per chrom and the three per-population states
echo "records per chromosome:"
bcftools index -s "$out"
echo "per-population value states (num = AC/AN shown, rule20 = AC/AN set to . by the lab, absent = ancestry not observed):"
bcftools query -f '%INFO/R20_AFR\t%INFO/R20_AMR\t%INFO/R20_EAS\t%INFO/R20_EUR\t%INFO/R20_OCE\t%INFO/R20_SAS\n' "$out" \
    | awk 'BEGIN{FS="\t"; split("AFR AMR EAS EUR OCE SAS", p, " ")}
        {for (i=1; i<=6; i++) {s = ($i=="1") ? "num" : (($i=="0") ? "rule20" : "absent"); c[p[i]"\t"s]++}}
        END{for (k in c) print k"\t"c[k]}' | sort
