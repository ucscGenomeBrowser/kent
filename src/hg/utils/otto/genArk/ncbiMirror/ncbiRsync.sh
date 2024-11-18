#!/bin/bash

usage() {
  printf "usage: ncbiRsync.sh <type>\n" 1>&2
  printf "where <type> is either GCF (==refseq) or GCA (==genbank)\n" 1>&2
  exit 255
}

if [ $# -ne 1 ]; then
  usage
fi

export type=$1

if [ "$type" != "GCF" ]; then
  if [ "$type" != "GCA" ]; then
    printf "specified <type> must be either GCF or GCA\n" 1>&2
    usage
  fi
fi 

export HGDB_CONF="/cluster/home/hiram/.hg.conf"

TOP="/hive/data/outside/ncbi/genomes/cronUpdates"

cd $TOP

YYYY=`date "+%Y"`
MM=`date "+%m"`
DD=`date "+%d"`
DS=`date "+%F"`
T=`date "+%T"`
epoch=`date "+%s"`

mkdir -p "$TOP/log/$YYYY/$MM"
export logFile="$TOP/log/$YYYY/$MM/$DS.$epoch.$type.log"

printf "### %s %s %s starting initial count scan\n" "${epoch}" "${DS}" "${T}" >> "${logFile}"

cd /hive/data/outside/ncbi/genomes
export countBefore=`find ./$type -mindepth 4 -maxdepth 4 -type d | awk -F'/' '{print $6}' | sort | wc -l`

epoch=`date "+%s"`
DS=`date "+%F"`
T=`date "+%T"`
LC_NUMERIC=en_US printf "### %s %s %s assemblies before rsync: %'d\n" "${epoch}" "${DS}" "${T}" "${countBefore}" >> "${logFile}"

time ( rsync -avPL --stats --prune-empty-dirs --exclude "suppressed/" \
     --timeout=1200 \
     --include "*/" --include "chr2acc" --include "*_rm.run" \
     --include "README_patch_release.txt" --include "*_feature_count.txt.gz" \
     --include "*_feature_table.txt.gz" --include "*.bed" \
     --include "*_pseudo_without_product.fna.gz" \
     --include "chr*.fna.gz" --include "*_localID2acc" \
     --include "README_*annotation_release_*" --include "README.txt" \
     --include "*chr2scaf" --include "assembly_status*" \
     --include "*_assembly_stats.txt" --include "*_genomic.gbff.gz" \
     --include "*placement.txt" --include "*definitions.txt" \
     --include "*_protein.faa.gz" --include "*_rna.gbff.gz" \
     --include "*_rna.fna.gz" --include "*_genomic_gaps.txt.gz" \
     --include "*_rm.out.gz" --include "*.agp.gz" --include "*regions.txt" \
     --include "*_genomic.fna.gz" --include "*.gff.gz" --include "*.gff" \
     --include "*_assembly_report.txt" --include "alt.scaf.fna.gz" \
     --include "unplaced.scaf.fna.gz" --exclude "*_cds_from_genomic_fna.gz" \
     --exclude "*_rna_from_genomic_fna.gz" \
     --exclude "*" rsync://ftp.ncbi.nlm.nih.gov/genomes/all/$type/ ./$type/ ) \
           >> ${logFile} 2>&1

epoch=`date "+%s"`
DS=`date "+%F"`
T=`date "+%T"`
printf "### %s %s %s completed rsync\n" "${epoch}" "${DS}" "${T}" >> "${logFile}"

export countAfter=`find ./$type -mindepth 4 -maxdepth 4 -type d | awk -F'/' '{print $6}' | sort | wc -l`
export newCount=`echo $countAfter $countBefore | awk '{printf "%d", $1 - $2}'`
epoch=`date "+%s"`
DS=`date "+%F"`
T=`date "+%T"`
LC_NUMERIC=en_US printf "### %s %s %s assemblies after rsync: %'d - %'d = %'d new assemblies\n" "${epoch}" "${DS}" "${T}" "${countAfter}" "${countBefore}" "${newCount}" >> "${logFile}"

# the extra tr and tail prevent gigantic outputs since a lot of the
# log is the progress counter from rsync which is one gigantic line
# separated by ctrl-M

printf "#### head -3 logFile ####\n" 1>&2
head -3 "${logFile}" | tr '' '\n' | tail -25 1>&2
printf "#### tail -25 logFile ####\n" 1>&2
tail -25 "${logFile}" | tr '' '\n' | tail -25 1>&2

cd /hive/data/outside/ncbi/genomes/reports
./fetch.sh $type >> "${logFile}" 2>&1

printf "#### tail -45 logFile ####\n" 1>&2
tail -45 "${logFile}" | tr '' '\n' | tail -45 1>&2
