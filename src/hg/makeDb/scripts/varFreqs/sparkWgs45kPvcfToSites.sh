#!/bin/bash
# Convert one SPARK WGS 2026_08 GLnexus pVCF chunk, or a position range of it,
# to an anonymous sites-only BCF with overall and ASD/non-ASD counts.
# Usage: sparkWgs45kPvcfToSites.sh <in.vcf.gz> <groups.txt> <out.bcf> [start end]
#   groups.txt: sample_id<TAB>AUT|NON_AUT, made from the sample metadata (see makeDoc)
#   start/end:  optional 1-based range [start, end); read from the unindexed
#               file with sparkWgs45kPvcfSlice.py. Without them the whole file.
# Steps: bcftools +fill-tags computes AC/AN/AF from the genotypes, and with -S
# also AC/AN/AF_AUT and _NON_AUT. view -G drops the genotypes. norm -m- splits
# multiallelic sites and left-aligns against hg38. Alleles with AC=0 (no carrier
# left after GLnexus genotype revision) are removed. VARLEN = len(ALT)-len(REF)
# is added so the track can be filtered by indel size.
set -euo pipefail

inVcf=$1
groups=$2
outBcf=$3
start=${4:-}
end=${5:-}
scriptDir=$(dirname "$(readlink -f "$0")")
refFa=/hive/data/genomes/hg38/bed/varFreqs/all/hg38.fa
# not the conda bcftools in ~max/software: it links libopenblas, which crashes
# under the parasol -ram address-space limit
BCFTOOLS=/cluster/software/src/bcftools-1.22/bcftools
export BCFTOOLS_PLUGINS=/cluster/software/src/bcftools-1.22/plugins
tmp=$outBcf.tmp.bcf

# MONOALLELIC records are alleles that GLnexus could not merge into an
# overlapping multiallelic site. In them only carriers have a called allele
# (e.g. ./1) and everyone else is ./., so fill-tags gives AN=1 or 2 and AF=1.
# GLnexus's own AF for them uses all samples, so set AN to 2 x the samples of
# each group and recompute AF from AC. Also add VARLEN.
nAll=$(( $(wc -l < "$groups") * 2 ))
nAut=$(( $(grep -c $'\tAUT$' "$groups") * 2 ))
nNon=$(( $(grep -c $'\tNON_AUT$' "$groups") * 2 ))
fixAwk='
BEGIN {FS = OFS = "\t"}
/^#CHROM/ {print "##INFO=<ID=VARLEN,Number=A,Type=Integer,Description=\"Length of ALT minus length of REF: >0 insertion, <0 deletion, 0 substitution\">"}
/^#/ {print; next}
$7 == "MONOALLELIC" {
    n = split($8, kv, ";")
    for (i = 1; i <= n; i++) {split(kv[i], p, "="); val[p[1]] = p[2]}
    val["AN"] = nAll; val["AN_AUT"] = nAut; val["AN_NON_AUT"] = nNon
    val["AF"] = sprintf("%.6g", val["AC"] / nAll)
    val["AF_AUT"] = sprintf("%.6g", val["AC_AUT"] / nAut)
    val["AF_NON_AUT"] = sprintf("%.6g", val["AC_NON_AUT"] / nNon)
    s = ""
    for (i = 1; i <= n; i++) {split(kv[i], p, "="); s = s (i > 1 ? ";" : "") p[1] "=" val[p[1]]}
    $8 = s
    delete val
}
{$8 = $8 ";VARLEN=" (length($5) - length($4)); print}'

# Only GT is needed. The other FORMAT fields (DP:AD:SB:GQ:PL:RNC) are 90% of the
# text, and PL grows with the square of the number of alleles: a 31-allele STR
# record is 95 MB with them, which made bcftools slow and ran a parasol job out
# of memory. perl strips every ':subfield' outside the header lines; columns 1-8
# of these files contain no ':' (INFO is only AF and AQ), and FORMAT becomes GT.
python3 "$scriptDir/sparkWgs45kPvcfSlice.py" "$inVcf" "${start:-1}" "${end:-2000000000}" \
  | perl -pe 's/:[^\t\n]*//g unless /^#/' \
  | $BCFTOOLS +fill-tags -- -t AC,AN,AF -S "$groups" \
  | $BCFTOOLS view -G -Ou \
  | $BCFTOOLS norm -m- --check-ref w -f "$refFa" -Ou 2> "$outBcf.norm.log" \
  | $BCFTOOLS view -e 'INFO/AC=0' -Ov \
  | awk -v nAll="$nAll" -v nAut="$nAut" -v nNon="$nNon" "$fixAwk" \
  | $BCFTOOLS view -Ob -o "$tmp"
$BCFTOOLS index "$tmp"
mv -f "$tmp" "$outBcf"
mv -f "$tmp.csi" "$outBcf.csi"
