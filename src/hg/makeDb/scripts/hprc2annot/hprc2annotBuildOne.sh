#!/bin/bash
# Build one contributed track for one HPRC Release 2 assembly.
# Usage: hprc2annotBuildOne.sh TRACK SAMPLE HAP ACC
#   TRACK  one of: cat liftoff censat censatCen pclai segdups methyl
#   SAMPLE e.g. HG00408      HAP 1|2      ACC GCA_041900255.1
# Downloads the source file from the public HPRC S3 bucket over HTTPS,
# converts it to a bigBed, and writes it into the hub assembly directory.
# Methylation is a bigWig and is copied over unchanged (no conversion).
# Appends a per-item stats line to $WORK/log/stats.tsv.
set -u -o pipefail

TRACK=$1; SAMPLE=$2; HAP=$3; ACC=$4

WORK=/hive/data/genomes/asmHubs/contrib/hprc2annot.build
HUB=/hive/data/genomes/asmHubs/contrib/hprc2annot
SCR=$HOME/kent/src/hg/makeDb/scripts/hprc2annot
IDX=$WORK/idx
S3BASE="https://human-pangenomics.s3-us-west-2.amazonaws.com"

# GenArk assembly dir for this accession (GCA/nnn/nnn/nnn/GCA_x.y)
p1=${ACC:4:3}; p2=${ACC:7:3}; p3=${ACC:10:3}
ADIR=/hive/data/genomes/asmHubs/GCA/$p1/$p2/$p3/$ACC
GBSIZES=$ADIR/$ACC.chrom.sizes.txt
if [ ! -s "$GBSIZES" ]; then echo "NO_SIZES $TRACK $ACC" >&2; exit 3; fi

OUT=$HUB/$ACC
mkdir -p "$OUT"
# Per-job scratch on LOCAL disk (fast, and keeps the multi-GB decompressed GFF3
# churn off the shared GPFS /hive). Override with HPRC2_SCRATCH.
SCRATCH=${HPRC2_SCRATCH:-/data/tmp/hprc2annot_scratch}
mkdir -p "$SCRATCH"
TMP=$(mktemp -d "$SCRATCH/j.$TRACK.$ACC.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

# PanSN chrom.sizes: prefix every genbank name with SAMPLE#HAP#
PANSIZES=$TMP/pansn.sizes
awk -v p="$SAMPLE#$HAP#" 'BEGIN{OFS="\t"}{print p$1,$2}' "$GBSIZES" > "$PANSIZES"

# S3 location from this track's index (join on sample_id + haplotype)
idxfile=$IDX/idx_$TRACK.csv
loc=$(awk -F, -v s="$SAMPLE" -v h="$HAP" '$1==s && $2==h{print $4; exit}' "$idxfile")
if [ -z "$loc" ]; then echo "NO_LOC $TRACK $SAMPLE $HAP" >&2; exit 4; fi
url="$S3BASE/${loc#s3://human-pangenomics/}"

dl() { # dl URL OUTFILE  (with retries; handles .gz transparently by caller)
  curl -sSL --retry 8 --retry-delay 3 --retry-all-errors --retry-connrefused \
       --connect-timeout 30 --max-time 1800 \
       "$1" -o "$2" || { echo "DL_FAIL $TRACK $ACC $1" >&2; return 5; }
}

# Filter rows against a chrom.sizes file, stdin to stdout. Two kinds of row are
# dropped and BOTH are counted, so a naming problem cannot pass silently:
#   - the sequence is in the file but the row ends past the sequence end
#   - the sequence name is absent from the file altogether
# Writes "overEnd<TAB>unmatchedRows<TAB>unmatchedNames" to $TMP/drop.n and, if
# any name went unmatched, a warning with the first few names to stderr.
sizeFilter() { # sizeFilter SIZESFILE
  awk -F'\t' -v cnt="$TMP/drop.n" -v track="$TRACK" -v acc="$ACC" '
    NR==FNR { sz[$1]=$2; next }
    !($1 in sz) { u++; if (!($1 in miss)) { miss[$1]=1; nMiss++;
                      if (nMiss<=5) ex = (ex=="" ? $1 : ex" "$1) } next }
    $3 <= sz[$1] { print; next }
    { d++ }
    END { printf "%d\t%d\t%d\n", d+0, u+0, nMiss+0 > cnt
          if (nMiss) printf "UNMATCHED_SEQ %s %s rows=%d names=%d e.g. %s\n",
                            track, acc, u, nMiss, ex > "/dev/stderr" }' "$1" -
}

# read the three counts sizeFilter left behind
readDrops() { read -r dropN unmatchedRows unmatchedNames < "$TMP/drop.n"; }

# a bigBed/bigWig conversion must not quietly produce an empty file
notEmpty() { # notEmpty FILE LABEL
  if [ ! -s "$1" ]; then echo "EMPTY_OUT $2 $TRACK $ACC" >&2; return 1; fi
}

stat() { # stat inputCount outputCount note
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$TRACK" "$ACC" "$SAMPLE" "$HAP" "$1" "$2" \
    "${dropN:-0}" "${unmatchedRows:-0}" "${unmatchedNames:-0}" "$3" \
    >> "$WORK/log/stats.tsv"
}

# gene tracks (cat, liftoff) share everything but the chrom.sizes flavour, the
# transcript-name source and the CDS-phase fixup, so keep the conversion in one
# place.
buildGenes() { # buildGenes GFF3 SIZESFILE OUTBB
  local gff=$1 sizes=$2 outbb=$3
  # awk only creates these on its first write, and the remap pass below reads
  # them as input files, so make sure they exist even for a GFF3 with no genes.
  : > "$TMP/gene.map"; : > "$TMP/tx.map"
  # one pass over the (large) GFF3: gene symbols/biotypes, transcript biotypes,
  # and the transcript count (the source of truth for the display fields)
  awk -F'\t' -v gm="$TMP/gene.map" -v tm="$TMP/tx.map" '
    $3=="gene"{ id=""; sym=""; bt="";
      n=split($9,a,";"); for(i=1;i<=n;i++){ split(a[i],kv,"="); if(kv[1]=="ID")id=kv[2]; else if(kv[1]=="gene_name")sym=kv[2]; else if(kv[1]=="gene_biotype")bt=kv[2] }
      if(id!="")print id"\t"sym"\t"bt > gm }
    $3=="transcript"||$3=="mRNA"{ id=""; tb=""; c++;
      n=split($9,a,";"); for(i=1;i<=n;i++){ split(a[i],kv,"="); if(kv[1]=="ID")id=kv[2]; else if(kv[1]=="transcript_biotype")tb=kv[2] }
      if(id!="")print id"\t"tb > tm }
    END{ print c+0 }' "$gff" > "$TMP/incount"
  inCount=$(cat "$TMP/incount")
  # -rnaNameAttr=ID keeps the transcript accession as the genePred name. Without
  # it gff3ToGenePred falls back to the gene, so every transcript of a gene ends
  # up with the same name and the transcript_biotype lookup below never matches.
  gff3ToGenePred -warnAndContinue -maxConvertErrors=-1 -rnaNameAttr=ID \
    "$gff" "$TMP/g.gp" 2>"$TMP/gp.log" || true
  notEmpty "$TMP/g.gp" genePred || return 6
  genePredToBigGenePred "$TMP/g.gp" "$TMP/g.bgp" \
    || { echo "GP2BGP_FAIL $TRACK $ACC" >&2; return 6; }
  notEmpty "$TMP/g.bgp" bigGenePred || return 6
  # remap: name2/geneName2 = gene symbol; type = transcript_biotype; geneType = gene_biotype
  awk -F'\t' 'BEGIN{OFS="\t"}
    FILENAME==g{ sym[$1]=$2; gbt[$1]=$3; next }
    FILENAME==t{ tbt[$1]=$2; next }
    { gid=$13; s=(gid in sym && sym[gid]!="")?sym[gid]:$13;
      $13=s; $19=s; $17=($4 in tbt)?tbt[$4]:""; $20=(gid in gbt)?gbt[gid]:""; print }' \
    g="$TMP/gene.map" t="$TMP/tx.map" "$TMP/gene.map" "$TMP/tx.map" "$TMP/g.bgp" > "$TMP/g.named"
  sizeFilter "$sizes" < "$TMP/g.named" | LC_COLLATE=C sort -k1,1 -k2,2n > "$TMP/g.sorted"
  readDrops
  notEmpty "$TMP/g.sorted" bed || return 6
  bedToBigBed -type=bed12+8 -tab -as=$SCR/bigGenePred.as -extraIndex=name,name2 \
    "$TMP/g.sorted" "$sizes" "$outbb" 2>"$TMP/bb.log" \
    || { echo "BB_FAIL $TRACK $ACC" >&2; cat "$TMP/bb.log" >&2; return 6; }
  outCount=$(wc -l < "$TMP/g.sorted")
}

case "$TRACK" in

cat)
  # CAT GFF3 uses bare GenBank sequence names, so it builds against the plain
  # GenArk chrom.sizes rather than the PanSN one.
  dl "$url" "$TMP/in.gff3.gz" || exit 5
  zcat "$TMP/in.gff3.gz" > "$TMP/in.gff3"
  buildGenes "$TMP/in.gff3" "$GBSIZES" "$OUT/catGenes.bb" || exit 6
  stat "$inCount" "$outCount" "transcripts"
  ;;

liftoff)
  dl "$url" "$TMP/in.gff3" || exit 5
  # liftoff CDS have no phase -> fill it, and ensure gff-version header is first
  { echo "##gff-version 3"; grep -v '^#' "$TMP/in.gff3"; } \
    | python3 $SCR/hprc2annotFillCdsPhase.py > "$TMP/phased.gff3"
  buildGenes "$TMP/phased.gff3" "$PANSIZES" "$OUT/liftoffGenes.bb" || exit 6
  stat "$inCount" "$outCount" "transcripts"
  ;;

censat)
  dl "$url" "$TMP/in.bed" || exit 5
  inCount=$(grep -vcE '^track|^#|^browser' "$TMP/in.bed")
  grep -vE '^track|^#|^browser' "$TMP/in.bed" \
    | sizeFilter "$PANSIZES" | LC_COLLATE=C sort -k1,1 -k2,2n > "$TMP/c.bed"
  readDrops
  notEmpty "$TMP/c.bed" bed || exit 6
  bedToBigBed -type=bed9 -tab -extraIndex=name "$TMP/c.bed" "$PANSIZES" "$OUT/censat.bb" 2>"$TMP/bb.log" \
    || { echo "BB_FAIL censat $ACC" >&2; cat "$TMP/bb.log" >&2; exit 6; }
  stat "$inCount" "$(wc -l < "$TMP/c.bed")" "satellite regions"
  ;;

censatCen)
  dl "$url" "$TMP/in.bed" || exit 5
  inCount=$(grep -vcE '^track|^#|^browser' "$TMP/in.bed")
  grep -vE '^track|^#|^browser' "$TMP/in.bed" \
    | awk -F'\t' 'BEGIN{OFS="\t"}{print $1,$2,$3}' \
    | sizeFilter "$PANSIZES" | LC_COLLATE=C sort -k1,1 -k2,2n > "$TMP/c.bed"
  readDrops
  notEmpty "$TMP/c.bed" bed || exit 6
  bedToBigBed -type=bed3 -tab "$TMP/c.bed" "$PANSIZES" "$OUT/censatCentromeres.bb" 2>"$TMP/bb.log" \
    || { echo "BB_FAIL censatCen $ACC" >&2; cat "$TMP/bb.log" >&2; exit 6; }
  stat "$inCount" "$(wc -l < "$TMP/c.bed")" "centromere regions"
  ;;

pclai)
  dl "$url" "$TMP/in.bed" || exit 5
  inCount=$(grep -vc '^#' "$TMP/in.bed")
  # -> bed9+3, name left blank (values shown on mouseover). Force thick = full item
  # (source has occasional thickStart=chromStart-1). Parse the source name
  # "SAMPLE/hN/<window>_(PC1,PC2)" into window + pca; col10 -> pcaSegment.
  grep -v '^#' "$TMP/in.bed" \
    | awk -F'\t' 'BEGIN{OFS="\t"}
        { seg=$4; sub(/^[^/]*\/[^/]*\//,"",seg); k=split(seg,b,"_"); pca=b[k];
          win=(pca!="")?substr(seg,1,length(seg)-length(pca)-1):seg;
          print $1,$2,$3,"",$5,$6,$2,$3,$9,win,pca,$10 }' \
    | sizeFilter "$PANSIZES" | LC_COLLATE=C sort -k1,1 -k2,2n > "$TMP/p.bed"
  readDrops
  notEmpty "$TMP/p.bed" bed || exit 6
  bedToBigBed -type=bed9+3 -tab -as=$SCR/pclai.as "$TMP/p.bed" "$PANSIZES" "$OUT/pclai.bb" 2>"$TMP/bb.log" \
    || { echo "BB_FAIL pclai $ACC" >&2; cat "$TMP/bb.log" >&2; exit 6; }
  stat "$inCount" "$(wc -l < "$TMP/p.bed")" "ancestry windows"
  ;;

segdups)
  dl "$url" "$TMP/in.bed" || exit 5
  inCount=$(grep -vc '^#' "$TMP/in.bed")
  # The paralog partner is reported as a PanSN name (SAMPLE#HAP#GenBank), which
  # the browser translates for the chrom column but not for a plain text field.
  # Build a PanSN -> display-name map from the GenArk chromAlias so the partner
  # reads chr2:... like the rest of the page. Prefer the "ucsc" column, fall
  # back to "genbank"; a name with neither is left as it came.
  ALIAS=$ADIR/$ACC.chromAlias.txt
  : > "$TMP/partner.map"
  if [ -s "$ALIAS" ]; then
    awk -F'\t' 'NR==1{ sub(/^# */,"",$0); n=split($0,h,"\t");
                       for(i=1;i<=n;i++){ if(h[i]=="hprcV2")p=i; else if(h[i]=="ucsc")u=i; else if(h[i]=="genbank")g=i } next }
                { key=(p?$p:""); val=(u&&$u!="")?$u:((g)?$g:"");
                  if(key!="" && val!="") print key"\t"val }' "$ALIAS" > "$TMP/partner.map"
  fi
  # 44-col SEDEF -> bed9+6, name left blank (values shown on mouseover). 1-based
  # awk cols: 1 chr1, 2 start1, 3 end1, 6 strand1, 9 color, 10 chr2, 11 start2,
  #   12 end2, 14 strand2, 16 aln_len, 24 fracMatch, 36 sat_bases, 37 unique_id,
  #   38 original.
  # strand is strand2, the orientation of the paralogous copy relative to this
  # one. strand1 (col 6) is "+" on every SEDEF row and carries no information;
  # using it made every inverted duplication render as forward.
  grep -v '^#' "$TMP/in.bed" \
    | awk -F'\t' -v pm="$TMP/partner.map" 'BEGIN{OFS="\t";
          while((getline line < pm) > 0){ split(line,m,"\t"); disp[m[1]]=m[2] } }
        { sc=int($24*1000); if(sc<0)sc=0; if(sc>1000)sc=1000;
          st=($14=="+"||$14=="-")?$14:".";
          pchr=($10 in disp)?disp[$10]:$10; part=pchr":"$11"-"$12;
          pct=sprintf("%.1f",$24*100);
          print $1,$2,$3,"",sc,st,$2,$3,$9,part,pct,$16,$36,$37,$38 }' \
    | sizeFilter "$PANSIZES" | LC_COLLATE=C sort -k1,1 -k2,2n > "$TMP/s.bed"
  readDrops
  notEmpty "$TMP/s.bed" bed || exit 6
  bedToBigBed -type=bed9+6 -tab -as=$SCR/segdups.as "$TMP/s.bed" "$PANSIZES" "$OUT/segdups.bb" 2>"$TMP/bb.log" \
    || { echo "BB_FAIL segdups $ACC" >&2; cat "$TMP/bb.log" >&2; exit 6; }
  stat "$inCount" "$(wc -l < "$TMP/s.bed")" "duplication calls"
  ;;

methyl)
  # ONT 5mC methylation bigWig: download as-is (PanSN names resolve via
  # chromAlias). Download to a hidden temp in the output dir, then atomic rename.
  tmpbw="$OUT/.methylation.bw.$$"
  dl "$url" "$tmpbw" || { rm -f "$tmpbw"; exit 5; }
  cc=$(bigWigInfo "$tmpbw" 2>/dev/null | awk '/chromCount/{print $2}')
  if [ -z "$cc" ]; then echo "BAD_BW $ACC" >&2; rm -f "$tmpbw"; exit 6; fi
  mv "$tmpbw" "$OUT/methylation.bw"
  dropN=0; unmatchedRows=0; unmatchedNames=0
  stat "$cc" "$cc" "bigWig sequences"
  ;;

*)
  echo "UNKNOWN_TRACK $TRACK" >&2; exit 2 ;;
esac

echo "OK $TRACK $ACC"
