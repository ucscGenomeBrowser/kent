#!/bin/bash
# Download the cancerhotspots.org tables and build the cancerHotspots bigBed.
# Run in the data directory, e.g.
#   cd /hive/data/genomes/hg38/bed/cancerVariants/cancerHotspots
#   ~/kent/src/hg/makeDb/scripts/cancerMutations/cancerHotspotsBuild.sh hg38
# usage: cancerHotspotsBuild.sh hg38|hg19
set -beEu -o pipefail

db=${1:?usage: cancerHotspotsBuild.sh hg38|hg19}
scriptDir=$(dirname $(readlink -f $0))
sizes=/hive/data/genomes/$db/chrom.sizes
case $db in
    hg38) gffUrl=https://ftp.ensembl.org/pub/release-115/gff3/homo_sapiens/Homo_sapiens.GRCh38.115.chr.gff3.gz
          chain=/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz ;;
    # the last Ensembl release on GRCh37, close to the Ensembl 75 transcripts of the v2 table
    hg19) gffUrl=https://ftp.ensembl.org/pub/grch37/release-87/gff3/homo_sapiens/Homo_sapiens.GRCh37.87.chr.gff3.gz
          chain=none ;;
    *) echo "unknown db $db"; exit 1 ;;
esac
gff=orig/$(basename $gffUrl)

# download into .part, then mv, so a re-run never appends to a replaced file
get() {
    url=$1; out=$2
    if [ -s "$out" ]; then echo "$out exists, skipping"; return; fi
    rm -f "$out.part"
    curl -sSfL -o "$out.part" "$url"
    mv "$out.part" "$out"
}

mkdir -p orig
# v3: the 164 new hotspots of Bandlamudi et al. 2026 (+ variant sheets that repeat v2)
get https://www.cancerhotspots.org/files/hotspots_v3.xlsx orig/hotspots_v3.xlsx
# v2: single residue, splice and in-frame indel hotspots of Chang et al. 2017, with hg19 positions
get https://www.cancerhotspots.org/files/hotspots_v2.xls orig/hotspots_v2.xls
# v2 mutation data, only used to find the transcript that the residue numbers refer to
get https://cbioportal-download.s3.amazonaws.com/cancerhotspots.v2.maf.gz orig/cancerhotspots.v2.maf.gz
get $gffUrl $gff

# most common transcript and Ensembl gene ID per gene symbol in the MAF (Ensembl 75)
zcat orig/cancerhotspots.v2.maf.gz | awk -F'\t' '
    $1=="Hugo_Symbol" {for (i=1;i<=NF;i++) col[$i]=i; next}
    /^#/ {next}
    $col["Transcript_ID"]!="" {print $1"\t"$col["Transcript_ID"]"\t"$col["Gene"]}' \
    | sort | uniq -c | sort -k2,2 -k1,1nr | awk '!s[$2]++ {print $2"\t"$3"\t"$4"\t"$1}' > mafGeneTx.tsv

uv run -q $scriptDir/cancerHotspotsToBed.py orig/hotspots_v2.xls orig/hotspots_v3.xlsx mafGeneTx.tsv \
    $gff /hive/data/genomes/$db/$db.2bit $chain cancerHotspots.unsorted.bed 2> cancerHotspots.log
tail -4 cancerHotspots.log
sort -k1,1 -k2,2n cancerHotspots.unsorted.bed > cancerHotspots.bed
bedToBigBed -tab -type=bed12+14 -as=$scriptDir/cancerHotspots.as cancerHotspots.bed $sizes cancerHotspots.bb
