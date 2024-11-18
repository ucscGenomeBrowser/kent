#!/bin/bash

if [ $# -ne 1 ]; then
  printf "usage: fetch.sh [GCA|GCF]\n" 1>&2
  exit 255
fi

export type=$1

cd /hive/data/outside/ncbi/genomes/reports

case $type in
  GCA)
    printf "# genbank\n" 1>&2
    rsync -a --stats -L rsync://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_genbank* ./
    grep -v "^#" assembly_summary_genbank.txt \
      | awk -F$'\t' '{gsub(" ", "_",$16); printf "%s\t%s\t%s_%s\t%s\n", $6,$7,$1,$16,$8}' > genbank.taxIds.txt
    ;;
  GCF)
    printf "# refseq\n"
    rsync -a --stats -L rsync://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_refseq* ./
    grep -v "^#" assembly_summary_refseq.txt \
      | awk -F$'\t' '{gsub(" ", "_",$16); printf "%s\t%s\t%s_%s\t%s\n", $6,$7,$1,$16,$8}' > refseq.taxIds.txt
    ;;
  *)
  printf "usage: fetch.sh [GCA|GCF]\n" 1>&2
  exit 255
    ;;
esac

for F in species_genome_size.txt.gz README_change_notice.txt README_assembly_summary.txt prokaryote_type_strain_report.txt ANI_report_prokaryotes.txt README_ANI_report_prokaryotes.txt README_indistinguishable_groups_prokaryotes.txt indistinguishable_groups_prokaryotes.txt
do
  rsync -a --stats -L \
     rsync://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/${F} ./
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

exit 0

rsync -a --stats -L rsync://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/ ./

exit $?

# already in the top level rsync
rsync -a --stats -L rsync://ftp.ncbi.nlm.nih.gov/genomes/README_assembly_summary.txt ./

exit $?

wget --timestamping \
ftp://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_genbank.txt

wget --timestamping \
ftp://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_refseq.txt


# -r--r--r-- 1      6800 Sep 22  2016 README_change_notice.txt
# -r--r--r-- 1     14648 Mar 15  2018 README_assembly_summary.txt
# -rw-r--r-- 1   2839114 Feb  4 03:00 prokaryote_type_strain_report.txt
# -r--r--r-- 1  54322248 Feb  4 04:36 ANI_report_bacteria.txt
# -r--r--r-- 1 174049665 Feb  4 04:36 assembly_summary_genbank.txt
# -r--r--r-- 1   4347396 Feb  4 04:36 assembly_summary_genbank_historical.txt
# -r--r--r-- 1  57602222 Feb  4 04:36 assembly_summary_refseq.txt
# -r--r--r-- 1   4191655 Feb  4 04:36 assembly_summary_refseq_historical.txt
# -rw-r--r-- 1     33218 Feb  4 05:30 species_genome_size.txt.gz

