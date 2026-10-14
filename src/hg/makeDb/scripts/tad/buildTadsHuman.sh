#!/bin/bash
# Build the human TAD bigBeds (tadsDixon, tadsSchmitt, tadsMcArthur) for hg19 and hg38.
# refs #21599
#
# usage: buildTadsHuman.sh [tadDir]
#   tadDir defaults to /hive/data/outside/tad; reads <tadDir>/<dataset>/source/*.bed (made by
#   extractTadsSupplements.py and the McArthur GitHub file) and writes
#   <tadDir>/<dataset>/build/{hg19,hg38}/*.bb plus *.unmapped and a per-stage count log.
#
# Rules applied to every set:
#   - liftOver with default -minMatch=0.95; unmapped rows are reported, not rescued.
#   - keep primary chroms only (chr1-22, X, Y, M), dropping alt/random/chrUn lifts.
#   - trim chromEnd to the chrom size (instead of bedClip, which drops the whole row).
#   - drop lifted items whose size is more than double their size in the source assembly
#     (the chain jumped across a gap, centromere or rearranged block inside the item).
set -beEu -o pipefail

tad=${1:-/hive/data/outside/tad}
asDir=$(cd "$(dirname "$0")" && pwd)     # .as files live next to this script in kent git
declare -A CHAIN=( [hg18.hg19]=/gbdb/hg18/liftOver/hg18ToHg19.over.chain.gz \
                   [hg18.hg38]=/gbdb/hg18/liftOver/hg18ToHg38.over.chain.gz \
                   [hg19.hg38]=/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz )
sizes() { echo /hive/data/genomes/$1/chrom.sizes; }

# Schmitt sheet code -> sample name, used as the item name (shown by the tadsSchmitt mouseOver)
declare -A SCHMITT=( [GM12878]="GM12878 lymphoblastoid cells" [H1]="H1 human embryonic stem cells"
  [IMR90]="IMR90 fetal lung fibroblasts" [MES]="H1-derived mesendoderm cells"
  [MSC]="H1-derived mesenchymal stem cells" [NPC]="H1-derived neural progenitor cells"
  [TRO]="H1-derived trophoblast-like cells" [AD]="Adrenal gland" [AO]="Aorta" [BL]="Bladder"
  [CO]="Dorsolateral prefrontal cortex" [HC]="Hippocampus" [LG]="Lung" [LI]="Liver"
  [LV]="Left ventricle of heart" [OV]="Ovary" [PA]="Pancreas" [PO]="Psoas muscle"
  [RV]="Right ventricle of heart" [SB]="Small bowel" [SX]="Spleen" )

# finish in.bed(N cols + srcSize col) db nCols -> out.bed ; prints counts
finish() {
    local in=$1 db=$2 out=$3 label=$4
    local nIn=$(wc -l < "$in")
    awk -F'\t' -v OFS='\t' 'NR==FNR{sz[$1]=$2; next}
        $1 !~ /^chr([0-9]+|X|Y|M)$/ {nonPrim++; next}
        {if ($3 > sz[$1]) {$3 = sz[$1]; trimmed++}
         if ($3 - $2 > 2 * $NF) {inflated++; print > "/dev/stderr"; next}
         NF--; print}
        END{printf "%d\t%d\t%d\n", nonPrim, trimmed, inflated > "'"$out"'.counts"}' \
        "$(sizes $db)" "$in" 2> "$out.inflated" | sort -k1,1 -k2,2n > "$out"
    read nonPrim trimmed inflated < "$out.counts"; rm "$out.counts"
    printf "%s\t%s\tin %d\tnonPrimary %d\ttrimmed %d\tinflated %d\tfinal %d\n" \
        "$label" "$db" "$nIn" "$nonPrim" "$trimmed" "$inflated" "$(wc -l < "$out")" | tee -a "$log"
}

# liftOrCopy src.bed(+srcSize) fromDb toDb bedPlus prefix -> prefix.pre.bed
liftOrCopy() {
    local src=$1 from=$2 to=$3 plus=$4 pre=$5
    if [ "$from" = "$to" ]; then
        cp "$src" "$pre.pre.bed"; : > "$pre.unmapped"
    else
        liftOver -bedPlus=$plus -tab "$src" "${CHAIN[$from.$to]}" "$pre.pre.bed" "$pre.unmapped"
    fi
    printf "%s\t%s\tsource %d\tunmapped %d\n" "$(basename $pre)" "$to" "$(wc -l < "$src")" \
        "$(grep -vc '^#' "$pre.unmapped" || true)" | tee -a "$log"
}

tmp=$(mktemp -d /data/tmp/tadsHuman.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# --- Dixon 2012 domains, hg18 source ---
d=$tad/dixon2012; log=$d/build/buildTadsHuman.log; mkdir -p $d/build; : > $log
for ct in hESC IMR90; do
    bb=tadsDixon$( [ $ct = hESC ] && echo HESC || echo IMR90 )
    awk -v OFS='\t' '{print $1,$2,$3,$4,$3-$2}' $d/source/dixon_$ct.hg18.bed > $tmp/src.bed
    for db in hg19 hg38; do
        mkdir -p $d/build/$db
        liftOrCopy $tmp/src.bed hg18 $db 4 $d/build/$db/$ct
        finish $d/build/$db/$ct.pre.bed $db $tmp/$ct.$db.bed $ct
        mv $tmp/$ct.$db.bed.inflated $d/build/$db/$ct.inflated
        bedToBigBed -tab -type=bed4 -as=$asDir/tadDomain.as $tmp/$ct.$db.bed $(sizes $db) \
            $d/build/$db/$bb.bb
    done
done

# --- Schmitt 2016 boundaries, native hg19 ---
d=$tad/schmitt2016; log=$d/build/buildTadsHuman.log; mkdir -p $d/build; : > $log
for code in "${!SCHMITT[@]}"; do
    awk -v OFS='\t' -v n="${SCHMITT[$code]}" '{print $1,$2,$3,n,$3-$2}' \
        $d/source/schmitt_$code.hg19.bed > $tmp/src.bed
    for db in hg19 hg38; do
        mkdir -p $d/build/$db
        liftOrCopy $tmp/src.bed hg19 $db 4 $d/build/$db/$code
        finish $d/build/$db/$code.pre.bed $db $tmp/$code.$db.bed $code
        mv $tmp/$code.$db.bed.inflated $d/build/$db/$code.inflated
        bedToBigBed -tab -type=bed4 -as=$asDir/tadBoundary.as $tmp/$code.$db.bed $(sizes $db) \
            $d/build/$db/tadsSchmitt$code.bb
    done
done

# --- McArthur 2021 boundary stability, native hg19 ---
# name = hg19 source bin, 1-based like the browser position box; score = round(contexts/37*1000)
d=$tad/mcarthur2021; log=$d/build/buildTadsHuman.log; mkdir -p $d/build; : > $log
tail -n +2 $d/source/100kbBookendBoundaries_byStability.bed \
  | awk -F'\t' -v OFS='\t' '{c=int($4+0);
      printf "%s\t%d\t%d\thg19 %s:%d-%d\t%d\t%d\t%.3f\t%d\n", $1,$2,$3, $1,$2+1,$3,
             int(c/37*1000+0.5), c, $5, $3-$2}' > $tmp/src.bed
for db in hg19 hg38; do
    mkdir -p $d/build/$db
    liftOrCopy $tmp/src.bed hg19 $db 5 $d/build/$db/mcarthur
    finish $d/build/$db/mcarthur.pre.bed $db $tmp/mcarthur.$db.bed mcarthur
    mv $tmp/mcarthur.$db.bed.inflated $d/build/$db/mcarthur.inflated
    bedToBigBed -tab -type=bed5+2 -as=$asDir/tadStability.as $tmp/mcarthur.$db.bed $(sizes $db) \
        $d/build/$db/tadsMcArthur.bb
done
rm -f $tad/*/build/hg*/*.pre.bed
