#!/bin/bash

if [ $# -ne 1 ]; then
  printf "usage: fetch.sh [GCA|GCF]\n" 1>&2
  exit 255
fi

export type=$1

cd /hive/data/outside/ncbi/genomes/reports

case "${type}" in
  GCA)
    printf "# genbank\n" 1>&2
    for T in ".txt" "_historical.txt"
    do
       rm -f "assembly_summary_genbank${T}"
       wget --timestamping "https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_genbank${T}"
    done
    grep -v "^#" assembly_summary_genbank.txt \
      | awk -F$'\t' '{gsub(" ", "_",$16); printf "%s\t%s\t%s_%s\t%s\n", $6,$7,$1,$16,$8}' > genbank.taxIds.txt
    /hive/data/outside/ncbi/genomes/reports/loadAssemblySummaries.sh "${type}"
    ;;
  GCF)
    printf "# refseq\n"
    for T in ".txt" "_historical.txt"
    do
       rm -f "assembly_summary_refseq${T}"
       wget --timestamping "https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_refseq${T}"
    done
    grep -v "^#" assembly_summary_refseq.txt \
      | awk -F$'\t' '{gsub(" ", "_",$16); printf "%s\t%s\t%s_%s\t%s\n", $6,$7,$1,$16,$8}' > refseq.taxIds.txt
    /hive/data/outside/ncbi/genomes/reports/loadAssemblySummaries.sh "${type}"
    ;;
  *)
  printf "usage: fetch.sh [GCA|GCF]\n" 1>&2
  exit 255
    ;;
esac

# for F in species_genome_size.txt.gz README_change_notice.txt README_assembly_summary.txt prokaryote_type_strain_report.txt ANI_report_prokaryotes.txt README_ANI_report_prokaryotes.txt README_indistinguishable_groups_prokaryotes.txt indistinguishable_groups_prokaryotes.txt

for F in species_genome_size.txt.gz README_change_notice.txt README_assembly_summary.txt prokaryote_type_strain_report.txt ANI_report_prokaryotes.txt README_ANI_report_prokaryotes.txt
do
  wget --timestamping \
     https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/${F}
  rm -f wget-*
done

case $type in
  GCA)
    cd /hive/data/outside/ncbi/genomes/reports/genbank
    ./updateLists.sh genbank
    ./catLists.sh genbank
    /hive/data/outside/ncbi/genomes/reports/newAsm/genbank.sh
    ;;
  GCF)
    cd /hive/data/outside/ncbi/genomes/reports/refseq
    ./updateLists.sh refseq
    ./catLists.sh refseq
    /hive/data/outside/ncbi/genomes/reports/newAsm/refseq.sh
    ;;
esac

# places everything in one single list for asmId to clade correspondence
/hive/data/outside/ncbi/genomes/reports/newAsm/cladesToday.sh

case $type in
  GCA)
    /hive/data/outside/ncbi/genomes/reports/allCommonNames/cronUpdate.sh
    ;;
esac
