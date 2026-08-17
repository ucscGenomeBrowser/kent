#!/bin/bash

set -beEu -o pipefail

usage() {
  printf "usage: fetchOne.sh <asmId>\n" 1>&2
  printf "where <asmId> is the full GCF_/GCA_ accession id, e.g.:\n\tGCA_028878055.2_NHGRI_mSymSyn1-v2.0_pri\n" 1>&2
  exit 255
}

if [ $# -ne 1 ]; then
  usage
fi

export TOP="/hive/data/outside/ncbi/genomes"

cd "${TOP}"

export asmId=$1
export gcX="${asmId:0:3}"
export d0="${asmId:4:3}"
export d1="${asmId:7:3}"
export d2="${asmId:10:3}"
export srcDir="${gcX}/${d0}/${d1}/${d2}/${asmId}"
# export srcDir0="${gcX}/${d0}/${d1}/${d2}"
# export srcDir0="${gcX}/${d0}"
export destDir="/hive/data/outside/ncbi/genomes/${srcDir}"
# export destDir0="/hive/data/outside/ncbi/genomes/${srcDir}"

# GCF/029/910/555/GCF_029910555.1_ASM2991055v1
# GCF/029/910/575/GCF_029910575.1_ASM2991057v1


printf "%s\n" "working: ${destDir}" 1>&2
printf "%s\n" "srcDir: ${srcDir}" 1>&2

printf "# lftp from ftp://ftp.ncbi.nlm.nih.gov/genomes/all/${srcDir}/\n" 1>&2
printf "# https://ftp.ncbi.nlm.nih.gov/genomes/all/${srcDir}/\n" 1>&2
printf "mkdir -p \"${destDir}\"\n" 1>&2

mkdir -p "${destDir}"
cd "${destDir}"

export startEpoch=`date "+%s"`

# lftp -e "open ftp://ftp.ncbi.nlm.nih.gov; ls /genomes/all/GCF/000/172/535/GCF_000172535.1_Blac_1.0/README.txt; quit"

# lftp mirror exclude-only approach:
# unlike rsync, lftp exclude rules override include rules regardless of order,
# so a catch-all --exclude-glob * would block everything.
# Instead, use -x (regex) to exclude only the unwanted items;
# everything else is downloaded by default.

lftp -e "
  open ftp://ftp.ncbi.nlm.nih.gov;
  set net:timeout 1200;
  set mirror:dereference yes;
  mirror --parallel=4 --verbose --only-newer --delete --no-perms \
    -x suppressed \
    -x Annotation_comparison \
    -x RefSeq_transcripts_alignments \
    -x RNASeq_coverage_graphs \
    -x '.*_ani_contam_ranges\.tsv' \
    -x '.*_ani_report\.txt' \
    -x '.*_fcs_report\.txt' \
    -x '.*_gene_ontology\.gaf\.gz' \
    -x '.*_genomic\.gtf\.gz' \
    -x '.*_protein\.gpff\.gz' \
    -x '.*_translated_cds\.faa\.gz' \
    -x '.*_wgsmaster\.gbff\.gz' \
    -x 'annotation_hashes\.txt' \
    -x 'md5checksums\.txt' \
    -x 'uncompressed_checksums\.txt' \
    /genomes/all/${srcDir}/ ./;
  quit"

export epoch=`date "+%s"`
export secondsET=`echo $epoch $startEpoch | awk '{printf "%d", $1-$2}'`
export DS=`date "+%F"`
export T=`date "+%T"`
printf "### %s %s %s completed lftp %d seconds %s\n" "${epoch}" "${DS}" "${T}" "${secondsET}" "${asmId}"
printf "%s\n" "done: ${srcDir}" 1>&2
exit $?
