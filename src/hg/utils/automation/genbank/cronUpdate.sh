#!/bin/bash

# exit on any command failure
set -beEu -o pipefail

function usage() {
  printf "usage: cronUpdate.sh <genbank|refseq> </path/to/local/copy/from/ncbi>\n" 1>&2
  printf "select genbank or refseq assemblies update\n" 1>&2
  printf "the '/path/to/local/copy/from/ncbi' is a local filesystem where\n" 1>&2
  printf "the copy from NCBI will accumulate.  To that path the asmType\n" 1>&2
  printf "will be added, for UCSC this is:\n" 1>&2
  printf "/hive/data/outside/ncbi/genomes/<genbank|refseq>/\n" 1>&2
}

if [ $# -ne 2 ]; then
  usage
  exit 255
fi

export asmType=$1

if [ "${asmType}" != "genbank" -a "${asmType}" != "refseq" ]; then
  printf "ERROR: select genbank or refseq assemblies update\n" 1>&2
  usage
  exit 255
fi

export outside="$2/${asmType}"
if [ ! -d "${outside}" ]; then
  printf "ERROR: expecting the local directory path to exist:\n" 1>&2
  printf "    %s\n" "${outside}"
  usage
  exit 255
fi

printf "# running asmType: %s\n" "${asmType}" 1>&2
printf "# into directory: %s\n" "${outside}" 1>&2
export dirList="vertebrate_mammalian vertebrate_other archaea bacteria fungi invertebrate other plant protozoa"
if [ "${asmType}" = "refseq" ]; then
  dirList="archaea bacteria fungi invertebrate mitochondrion plant plasmid plastid protozoa vertebrate_mammalian vertebrate_other viral"
fi

##########################################################################
##  the '|| /bin/true' prevents this script from exit on rsync errors
ncbiRsync() {
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/${asmType}/assembly_summary_${asmType}.txt ./
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/${asmType}/README.txt ./

for D in ${dirList}
do
  mkdir -p ${D}
  printf "##########  working: %s  #########################\n" "${D}" 1>&2
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
export rsyncLogFile="logs/${YYYY}/${MM}/${DS}.rsync.log"
export currentFnaList="${YYYY}/${MM}/${DS}.genomicFna.list"
mkdir -p "logs/${YYYY}/${MM}"
mkdir -p "fileListings/${YYYY}/${MM}"
printf "# rsync log: %s\n" "${rsyncLogFile}" 1>&2
printf "# file list: %s\n" "fileListings/${currentFnaList}" 1>&2
time (ncbiRsync) > "${rsyncLogFile}" 2>&1
## construct list of *_genomic.fna.gz files, to be used in processing everything
find . -type f | grep "_genomic.fna.gz" \
   | sed -e 's#^./##;' | sort > "fileListings/${currentFnaList}" 2>&1
export sumCurrent=`cat "fileListings/${currentFnaList}" | sum`
if [ -s "fileListings/latest.genomicFna.list" ]; then
  sumPrevious=`cat "fileListings/latest.genomicFna.list" | sum`
  if [ "${sumCurrent}" = "${sumPrevious}" ]; then
    printf "# genomic.fna.gz list current == previous - '%s' == '%s'\n" "${sumCurrent}" "${sumPrevious}" 1>&2
    rm -f "fileListings/${currentFnaList}"
  else
    printf "# genomic.fna.gz list current != previous - '%s' != '%s'\n" "${sumCurrent}" "${sumPrevious}" 1>&2
    # diff exits non-zero, use the '|| true' to move on despite that 'fail' code
    (diff "fileListings/${currentFnaList}" \
       "fileListings/latest.genomicFna.list" | grep "^< " \
         | sed -e 's/^< //; s#/.*##;' | sort | uniq -c 1>&2) || true
    rm -f "fileListings/latest.genomicFna.list"
    ln -s "${currentFnaList}" "fileListings/latest.genomicFna.list"
  fi
fi
