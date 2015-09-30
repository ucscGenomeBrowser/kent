#!/bin/bash

# exit on any command failure
set -beEu -o pipefail

# should pick up this variable from a configuration file
export outside="/hive/data/outside/ncbi/genomes/genbank"

##########################################################################
##  the '|| /bin/true' prevents this script from exit on rsync errors
##         the genbank hierarchy contains unresolved symlinks
genbankRsync() {
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/assembly_summary_genbank.txt ./
rsync -avPL \
  rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/README.txt ./

for D in vertebrate_mammalian vertebrate_other archaea bacteria fungi \
         invertebrate other plant protozoa
do
  mkdir -p ${D}
  echo "##########  working: ${D}  #########################" 1>&2
  rsync -avPL --include "*/" --include="chr2acc" --include="*chr2scaf" \
     --include "*placement.txt" --include "*definitions.txt" \
     --include "*_rm.out.gz" --include "*agp.gz" --include "*regions.txt" \
     --include "*_genomic.fna.gz" --include "*gff.gz" \
     --include "*_assembly_report.txt" --exclude "*" \
          rsync://ftp.ncbi.nlm.nih.gov/genomes/genbank/${D}/ ./${D}/ \
     || /bin/true
done
}
##########################################################################
##########################################################################
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
time (genbankRsync) > "${rsyncLogFile}" 2>&1
## construct list of *_genomic.fna.gz files, to be used in processing everything
find . -type f | grep "_genomic.fna.gz" \
   | sed -e 's#^./##;' | sort > "${fileList}" 2>&1
rm -f "fileListings/latest.genomicFna.list"
ln -s "${YYYY}/${MM}/${DS}.genomicFna.list" "fileListings/latest.genomicFna.list"
cat "${fileList}" | sed -e 's/_genomic.fna.gz/_assembly_report.txt/' \
    | xargs sum > "${asmRptSum}"
rm -f "fileListings/latest.assembly_report.sum"
ln -s "${YYYY}/${MM}/${DS}.assembly_report.sum" "fileListings/latest.assembly_report.sum"
