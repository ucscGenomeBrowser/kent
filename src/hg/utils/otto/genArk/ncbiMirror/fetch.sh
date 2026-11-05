#!/bin/bash

if [ $# -ne 1 ]; then
  printf "usage: fetch.sh [GCA|GCF]\n" 1>&2
  exit 255
fi

export type=$1

HGSQL=/cluster/bin/x86_64/hgsql
DB=genark

# row counts of the assembly_summary files and the genark tables loaded
# from them, reported before and after the update.  These lines all start
# with '### size ' so ncbiRsync.sh can grep them out of its log.
fileRows() {
  if [ -s "$1" ]; then grep -vc '^#' "$1" || true; else echo 0; fi
}

tableRows() {
  ${HGSQL} -N ${DB} -e "select count(*) from $1;" 2>/dev/null || echo 0
}

declare -A beforeRows

# $1 = before|after, $2 = file or table name, $3 = row count
sizeLine() {
  local when="$1" name="$2" rows="$3"
  if [ "${when}" = "before" ]; then
    beforeRows["${name}"]="${rows}"
    LC_NUMERIC=en_US printf "### size before %s: %'d\n" "${name}" "${rows}"
  else
    LC_NUMERIC=en_US printf "### size after  %s: %'d (%+'d)\n" "${name}" "${rows}" \
      "$((rows - ${beforeRows[${name}]:-0}))"
  fi
}

# $1 = before|after, $2 = genbank|refseq, $3 = Genbank|Refseq
sizeReport() {
  local when="$1" src="$2" tbl="$3"
  sizeLine "${when}" "assembly_summary_${src}.txt" \
    "$(fileRows assembly_summary_${src}.txt)"
  sizeLine "${when}" "assembly_summary_${src}_historical.txt" \
    "$(fileRows assembly_summary_${src}_historical.txt)"
  sizeLine "${when}" "${DB}.assemblySummary${tbl}" \
    "$(tableRows assemblySummary${tbl})"
  sizeLine "${when}" "${DB}.assemblySummary${tbl}Historical" \
    "$(tableRows assemblySummary${tbl}Historical)"
}

cd /hive/data/outside/ncbi/genomes/reports

case "${type}" in
  GCA)
    printf "# genbank\n" 1>&2
    sizeReport before genbank Genbank
    for T in ".txt" "_historical.txt"
    do
       rm -f "assembly_summary_genbank${T}"
       wget --timestamping "https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_genbank${T}"
    done
    grep -v "^#" assembly_summary_genbank.txt \
      | awk -F$'\t' '{gsub(" ", "_",$16); printf "%s\t%s\t%s_%s\t%s\n", $6,$7,$1,$16,$8}' > genbank.taxIds.txt
    /hive/data/outside/ncbi/genomes/reports/loadAssemblySummaries.sh "${type}"
    sizeReport after genbank Genbank
    ;;
  GCF)
    printf "# refseq\n"
    sizeReport before refseq Refseq
    for T in ".txt" "_historical.txt"
    do
       rm -f "assembly_summary_refseq${T}"
       wget --timestamping "https://ftp.ncbi.nlm.nih.gov/genomes/ASSEMBLY_REPORTS/assembly_summary_refseq${T}"
    done
    grep -v "^#" assembly_summary_refseq.txt \
      | awk -F$'\t' '{gsub(" ", "_",$16); printf "%s\t%s\t%s_%s\t%s\n", $6,$7,$1,$16,$8}' > refseq.taxIds.txt
    /hive/data/outside/ncbi/genomes/reports/loadAssemblySummaries.sh "${type}"
    sizeReport after refseq Refseq
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
