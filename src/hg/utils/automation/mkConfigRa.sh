#!/bin/bash

# exit immediately upon any error anywhere
set -beEu -o pipefail

usage() {
   echo "usage: mkConfigRa.sh /hive/data/genomes/<db> \\
  <genomeClade> \\
  <genomeCladePriority> \\
  <commonName> <dbDbDir> \\
  <genomeId> \\
  <bioprojectId> \\
  <assemblyId> \\
  <mitoAcc> \\
  <orderKey> \\
  <photoCreditURL> \\
\"<photoCreditName>\" > <db>.config.ra" 1>&2
   echo "" 1>&2
   echo "expecting to find the ./genbank/ASSEMBLY_INFO file at that directory path" 1>&2
   echo "and no <db>.config.ra file there." 1>&2
   echo 'example:
export genomeClade="vertebrate"
export genomeCladePriority="60"
export commonName="Parrot"
export dbDbDir="birds"
export genomeId="15170"
export bioprojectId="171587"
export assemblyId="529068"
export mitoAcc="none"
export orderKey="4310"
export photoCreditURL="http://www.fws.gov/caribbean/ES/Parrot-Gallery.html"
export photoCreditName="Tom MacKenzie, U.S. Fish and Wildlife Service"

$HOME/kent/src/hg/utils/automation/mkConfigRa.sh /hive/data/genomes/amaVit1 \
    ${genomeClade} ${genomeCladePriority} ${commonName} ${dbDbDir} \
        ${genomeId} ${bioprojectId} ${assemblyId} ${mitoAcc} ${orderKey} \
        ${photoCreditURL} "${photoCreditName}" > amaVit1.config.ra' 1>&2
   exit 255
}

if [ $# -ne 12 ]; then
   usage
fi

export workDir=$1
export genomeClade=$2
export genomeCladePriority=$3
export commonName=$4
export dbDbSpeciesDir=$5
export genomeId=$6
export bioprojectId=$7
export assemblyId=$8
export mitoAcc=$9
export orderKey=${10}
export photoCreditURL=${11}
export photoCreditName=${12}
export db=`basename ${workDir}`
export genbankDir="${workDir}/genbank"

if [ ! -s ${workDir}/genbank/ASSEMBLY_INFO ]; then
    echo "ERROR: can not find file:" 1>&2
    echo "  ${workDir}/genbank/ASSEMBLY_INFO" 1>&2
    usage
fi

if [ -s ${workDir}/$db.config.ra ]; then
    echo "ERROR: already existing file:" 1>&2
    echo "  ${workDir}/$db.config.ra" 1>&2
    usage
fi

echo "# Config parameters for makeGenomeDb.pl:"
echo -e "db\t$db"
echo -e "clade\t$genomeClade"
echo -e "genomeCladePriority\t$genomeCladePriority"
echo -e "orderKey\t$orderKey"
echo -e "commonName\t$commonName"
echo -e "dbDbSpeciesDir\t${dbDbSpeciesDir}"
grep "^ORGANISM:" ${genbankDir}/ASSEMBLY_INFO | sed -e "s/ORGANISM:/scientificName/"
grep "^DATE:" ${genbankDir}/ASSEMBLY_INFO | awk '{print $2}' \
   | awk -F'-' '{printf "assemblyDate\t%s. %d\n", $2, $3}'
export shortName=`grep "^ASSEMBLY SHORT NAME:" ${genbankDir}/ASSEMBLY_INFO \
   | sed -e "s/ASSEMBLY SHORT NAME:\t//"`
export longName=`grep "^ASSEMBLY LONG NAME:" ${genbankDir}/ASSEMBLY_INFO \
   | sed -e 's/ASSEMBLY LONG NAME:\t//'`
export submitter=`grep "^ASSEMBLY SUBMITTER:" ${genbankDir}/ASSEMBLY_INFO \
   | sed -e 's/ASSEMBLY SUBMITTER:\t//'`
echo -e "assemblyLabel\t${submitter} ${shortName}"
echo -e "assemblyShortLabel\t${shortName}"
echo -e "fastaFiles\t${workDir}"'/ucsc/*.fa.gz'
echo -e "agpFiles\t${workDir}"'/ucsc/*.agp'
echo -e "mitoAcc\t${mitoAcc}"
echo "# qualFiles none"
echo -e "photoCreditURL\t${photoCreditURL}"
echo -e "photoCreditName\t${photoCreditName}"
echo -e "ncbiGenomeId\t${genomeId}"
echo -e "ncbiAssemblyName\t${longName}"
grep "^ASSEMBLY ACCESSION:" ${genbankDir}/ASSEMBLY_INFO \
   | sed -e "s/ASSEMBLY ACCESSION:/genBankAccessionID/"
echo -e "ncbiAssemblyId\t${assemblyId}"
echo -e "ncbiBioProject\t${bioprojectId}"
grep "^TAXID:" ${genbankDir}/ASSEMBLY_INFO \
   | sed -e "s/TAXID:/taxId/"

exit $?

# # qualFiles none
# photoCreditURL http://www.cas.vanderbilt.edu/bioimages/contact/baskauf-contact.htm
# photoCreditName &copy; 2003 Steve Baskauf
