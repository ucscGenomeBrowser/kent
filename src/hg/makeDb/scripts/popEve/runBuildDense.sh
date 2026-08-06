#!/bin/bash
# popEVE hg38 DENSE build: full per-amino-acid matrix from the transcript CSVs, with
# genomic coordinates reused from the VCF-derived popEve_sorted.tsv. Proteins absent from
# the CSV fall back to the sparse (single-nucleotide-reachable) VCF scores.
set -e
set -o pipefail   # so a bedToBigBed failure piped to tail is not masked
export PATH=$PATH:$HOME/bin/x86_64
cd /hive/data/genomes/hg38/bed/popEve
KS=$HOME/kent/src/hg/makeDb/scripts/popEve
CSVDIR=/hive/data/outside/popEve/popeve_csv/popEVE_ukbb_20250312
CHROMSIZES=/hive/data/genomes/hg38/chrom.sizes

echo "[$(date +%T)] 1. proteins not in the CSV (sparse fallback)"
ls "$CSVDIR" | sed 's/\.csv$//' | sort -u > csv_prots.txt
cut -f1 popEve_sorted.tsv | sort -u > all_prots.txt
comm -13 csv_prots.txt all_prots.txt > noncsv_prots.txt
echo "  CSV proteins: $(wc -l < csv_prots.txt)  non-CSV (sparse): $(wc -l < noncsv_prots.txt)"

echo "[$(date +%T)] 2. color anchors over the full matrix"
find "$CSVDIR" -name '*.csv' -print0 | xargs -0 awk -F, 'FNR>1 && $3!="nan" && $3!="" {print $3}' > allscores.txt
awk -F'\t' 'NR==FNR{np[$1];next} ($1 in np) && $8!="nan" && $8!="" {print $8}' \
    noncsv_prots.txt popEve_sorted.tsv >> allscores.txt
echo "  score values: $(wc -l < allscores.txt)"
sort -n -S 4G -T sorttmp allscores.txt > allscores_sorted.txt
N=$(wc -l < allscores_sorted.txt)
LOI=$(( (N-1)*5/1000 + 1 ))     # p0.5  (1-based line)
HII=$(( (N-1)*995/1000 + 1 ))   # p99.5
MEDI=$(( (N-1)/2 + 1 ))
LO=$(awk -v i=$LOI 'NR==i{printf "%.3f",$1; exit}' allscores_sorted.txt)
HI=$(awk -v i=$HII 'NR==i{printf "%.3f",$1; exit}' allscores_sorted.txt)
MED=$(awk -v i=$MEDI 'NR==i{printf "%.3f",$1; exit}' allscores_sorted.txt)
echo "$LO $HI" > anchorsDense.txt
echo "  n=$N  p0.5=$LO  median=$MED  p99.5=$HI"

echo "[$(date +%T)] 3. convert (dense)"
python3 $KS/vcfToPopEveHeatmap.py popEve_sorted.tsv np_strand.tsv popEve_dense_raw.bed \
    $LO $HI "$CSVDIR" 2> convert_dense.log
tail -1 convert_dense.log

echo "[$(date +%T)] 4. bedSort + filter + bigBed"
bedSort popEve_dense_raw.bed popEve_dense_sorted.bed
awk 'NR==FNR{ok[$1]=1;next} $1 in ok' $CHROMSIZES popEve_dense_sorted.bed > popEve_dense_filtered.bed
bedToBigBed -type=bed12+ -tab -as=$KS/popEve_heatmap.as popEve_dense_filtered.bed \
    $CHROMSIZES popEve.bb 2>&1 | tail -1
echo "[$(date +%T)] done; proteins=$(wc -l < popEve_dense_filtered.bed)  size=$(stat -c %s popEve.bb)"
