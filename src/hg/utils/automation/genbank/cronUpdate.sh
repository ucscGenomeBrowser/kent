#!/bin/bash

# exit on any command failure
set -beEu -o pipefail

function usage() {
  printf "usage: cronUpdate.sh <genbank|refseq>\n" 1>&2
  printf "select genbank or refseq assemblies update\n" 1>&2
}

if [ $# -ne 1 ]; then
  usage
  exit 255
fi

export asmType=$1

if [ "${asmType}" != "genbank" -a "${asmType}" != "refseq" ]; then
  printf "ERROR: select genbank or refseq assemblies update\n" 1>&2
  usage
  exit 255
fi

printf "running asmType: %s\n" "${asmType}" 1>&2

# should pick up this variable from a configuration file
export outside="/hive/data/outside/ncbi/genomes/${asmType}"

##########################################################################
##  the '|| /bin/true' prevents this script from exit on rsync errors
ncbiRsync() {
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/${asmType}/assembly_summary_${asmType}.txt ./
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/${asmType}/README.txt ./

for D in archaea bacteria fungi invertebrate mitochondrion plant plasmid \
  plastid protozoa vertebrate_mammalian vertebrate_other viral
do
  mkdir -p ${D}
  echo "##########  working: ${D}  #########################" 1>&2
  rsync -avPL --include "*/" --include="chr2acc" --include="*chr2scaf" \
     --include "*placement.txt" --include "*definitions.txt" \
     --include "*_rm.out.gz" --include "*agp.gz" --include "*regions.txt" \
     --include "*_genomic.fna.gz" --include "*gff.gz" \
     --include "*_assembly_report.txt" --exclude "*" \
          rsync://ftp.ncbi.nlm.nih.gov/genomes/${asmType}/${D}/ ./${D}/ \
     || /bin/true
done
}
##########################################################################
##########################################################################
# FTP directory listing of genomes/refseq:
#
# -r--r--r--   1 ftp      anonymous    12490 Sep 14 18:26 README.txt
# dr-xr-xr-x 400 ftp      anonymous    36864 Oct  5 07:55 archaea
# lr--r--r--   1 ftp      anonymous       47 Oct  1  2014 assembly_summary_refseq.txt -> ../ASSEMBLY_REPORTS/assembly_summary_refseq.txt
# dr-xr-xr-x 8903 ftp      anonymous   712704 Oct  5 07:55 bacteria
# dr-xr-xr-x 175 ftp      anonymous    16384 Oct  5 07:57 fungi
# dr-xr-xr-x  88 ftp      anonymous     8192 Oct  5 23:39 invertebrate
# lr--r--r--   1 ftp      anonymous       37 Oct  5 13:11 mitochondrion -> ../../../refseq/release/mitochondrion
# dr-xr-xr-x  66 ftp      anonymous     8192 Oct  5 07:58 plant
# lr--r--r--   1 ftp      anonymous       31 Oct  5 13:11 plasmid -> ../../../refseq/release/plasmid
# lr--r--r--   1 ftp      anonymous       31 Oct  5 13:11 plastid -> ../../../refseq/release/plastid
# dr-xr-xr-x  72 ftp      anonymous     8192 Oct  5 07:58 protozoa
# dr-xr-xr-x  90 ftp      anonymous     8192 Oct  5 07:58 vertebrate_mammalian
# dr-xr-xr-x  89 ftp      anonymous     8192 Oct  5 07:58 vertebrate_other
# dr-xr-xr-x 4799 ftp      anonymous   397312 Oct  5 07:58 viral
#
##########################################################################

## working here ##
cd "${outside}"

YYYY=`date "+%Y"`
MM=`date "+%m"`
DS=`date "+%FT%T"`
rsyncLogFile="logs/${YYYY}/${MM}/${DS}.rsync.log"
fileList="fileListings/${YYYY}/${MM}/${DS}.genomicFna.list"
asmRptSum="fileListings/${YYYY}/${MM}/${DS}.assembly_report.sum"
mkdir -p "logs/${YYYY}/${MM}"
mkdir -p "fileListings/${YYYY}/${MM}"
echo "# rsync log: ${rsyncLogFile}" 1>&2
echo "# file list: ${fileList}" 1>&2
time (ncbiRsync) > "${rsyncLogFile}" 2>&1
## construct list of *_genomic.fna.gz files, to be used in processing everything
find . -type f | grep "_genomic.fna.gz" \
   | sed -e 's#^./##;' | sort > "${fileList}" 2>&1
rm -f "fileListings/latest.genomicFna.list"
ln -s "${YYYY}/${MM}/${DS}.genomicFna.list" "fileListings/latest.genomicFna.list"
cat "${fileList}" | sed -e 's/_genomic.fna.gz/_assembly_report.txt/' \
    | xargs sum > "${asmRptSum}"
rm -f "fileListings/latest.assembly_report.sum"
ln -s "${YYYY}/${MM}/${DS}.assembly_report.sum" "fileListings/latest.assembly_report.sum"
