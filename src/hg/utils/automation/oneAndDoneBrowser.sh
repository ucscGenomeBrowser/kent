#!/bin/bash

set -beEu -o pipefail

if [ $# -ne 5 ]; then
  printf "usage: oneAndDoneBrowser.sh asmId dbName clade hgCentralClade trackDbDir

Build an assembly browser within the standard UCSC browser hierarchies
of /hive/data/genomes/dbName/ and with a dbDb hgcentral insert statement.

arguments:

asmId is a full assembly ID such as: GCF_000857045.1_ViralProj15142
dbName is the name and directory to build into /hive/data/genomes/dbName

clade is one of:
  primate mammal fish bird vertebrate invertebrate fungi
  plant nematode drosophila virus archaea bacteria

hgCentralClade is one of:
  ancestor bacteria ciliate deuterostome haplotypes insect mammal other protista
  simulation vertebrate virus worm

trackDbDir is one of the directories under makeDb/trackDb/<trackDbDir>/
  where this genome trackDb/<trackDbDir>/dbName/trackDb.ra will exist
" 1>&2
  exit 255
fi

#### default build parameters, will be adjusted below depending upon clade
export augustusSpecies="-augustusSpecies=human"
export ncbiRmsk="-ncbiRmsk"
export noRmsk=""
# export ucscNames="-ucscNames"
export ucscNames=""
####

export asmId="${1}"
export dbName="${2}"
export clade="${3}"
export hgCentralClade="${4}"
export trackDbDir="${5}"

export export gcX=${asmId:0:3}
export d0=${asmId:4:3}
export d1=${asmId:7:3}
export d2=${asmId:10:3}
export srcDir="/hive/data/outside/ncbi/genomes/${gcX}/${d0}/${d1}/${d2}/$asmId"

if [ ! -d "${srcDir}" ]; then
  printf "ERROR: can not find source directory:\n%s\n" "${srcDir}" 1>&2
  exit 255
fi
export asmReport="${srcDir}/${asmId}_assembly_report.txt"
if [ ! -s "${asmReport}" ]; then
  printf "ERROR: can not find the assembly report %s_assembly_report.txt\n" "${asmId}" 1>&2
  printf "in the source directory\n%s\n" "${srcDir}" 1>&2
  exit 255
fi

export sciName=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *//i; s/ *(.*//;'`
export organism=`grep -i 'organism name:' ${asmReport} | head -1 | tr -d "\r" | sed -e 's/.*organism name: *.*(//i; s/).*//;'`
export rmskSpecies="${sciName}"

export buildDir="/hive/data/genomes/${dbName}"
if [ ! -d "${buildDir}" ]; then
  mkdir "${buildDir}"
fi

case "$clade" in
  primate)
    ;;
  mammal)
    ;;
  fish)
    augustusSpecies="-augustusSpecies=zebrafish"
    ;;
  bird)
    augustusSpecies="-augustusSpecies=chicken"
    ;;
  vertebrate)
    ;;
  invertebrate)
    ;;
  fungi)
    augustusSpecies="-augustusSpecies=saccharomyces"
    ;;
  plant)
    augustusSpecies="-augustusSpecies=arabidopsis"
    ;;
  nematode)
    augustusSpecies="-augustusSpecies=caenorhabditis"
    ;;
  drosophila)
    augustusSpecies="-augustusSpecies=fly"
    ;;
  virus)
    rmskSpecies="viruses"
    augustusSpecies="-noAugustus -noXenoRefSeq"
    ;;
  archaea)
    noRmsk="-noRmsk"
    augustusSpecies="-noAugustus -noXenoRefSeq"
    ;;
  bacteria)
    noRmsk="-noRmsk"
    augustusSpecies="-noAugustus -noXenoRefSeq"
    ;;
   *)
    printf "ERROR: unrecognized clade: '%s'\n" "${clade}" 1>&2
    printf "must be one of:\n" 1>&2
    printf "  primate mammal fish bird vertebrate invertebrate fungi\n  plant nematode drosophila virus archaea bacteria\n" 1>&2
    exit 255
    ;;
esac

printf "# ==== %s ====\n" "`date '+%F %T %s'`" 1>&2
printf "# working in %s\n" "${buildDir}" 1>&2
printf "# building %s - %s\n" "${organism}" "${sciName}" 1>&2
printf "# dbName: %s\n" "${dbName}" 1>&2
printf "# ucscNames: %s\n" "${ucscNames}" 1>&2
printf "# rmskSpecies: %s\n" "${rmskSpecies}" 1>&2
printf "# augustusSpecies: %s\n" "${augustusSpecies}" 1>&2
printf "# ncbiRmsk: %s\n" "${ncbiRmsk}" 1>&2
if [ "x${noRmsk}y" != "xy" ]; then
  printf "# noRmsk: '%s'\n" "${noRmsk}" 1>&2
fi
printf "\n" 1>&2

# possible steps in order:

# download sequence assemblyGap chromAlias gatewayPage cytoBand gc5Base
# repeatMasker simpleRepeat allGaps idKeys windowMasker addMask gapOverlap
# tandemDups cpgIslands ncbiGene ncbiRefSeq xenoRefGene augustus trackDb cleanup

export stepStart="download"
export stepEnd="trackDb"

printf "cd \"${buildDir}\"\n" 1>&2
cd "${buildDir}"

printf "\$HOME/kent/src/hg/utils/automation/doAssemblyHub.pl \\
  -continue=\"${stepStart}\" -stop=\"${stepEnd}\" -dbName=\"${dbName}\" \\
     -rmskSpecies=\"${rmskSpecies}\" -bigClusterHub=ku -buildDir=\`pwd\` \\
        -fileServer=hgwdev -smallClusterHub=hgwdev \\
           ${noRmsk} ${ncbiRmsk} ${ucscNames} ${augustusSpecies} \\
              -workhorse=hgwdev \"${asmId}\" >> build.log 2>&1\n" 1>&2

$HOME/kent/src/hg/utils/automation/doAssemblyHub.pl \
  -continue="${stepStart}" -stop="${stepEnd}" -dbName="${dbName}" \
     -rmskSpecies="${rmskSpecies}" -bigClusterHub=ku -buildDir=`pwd` \
        -fileServer=hgwdev -smallClusterHub=hgwdev \
           ${noRmsk} ${ncbiRmsk} ${ucscNames} ${augustusSpecies} \
              -workhorse=hgwdev "${asmId}" >> build.log 2>&1

cd "${buildDir}"

if [ "download/${asmId}_assembly_report.txt" -nt "${dbName}.config.ra" ]; then

$HOME/kent/src/hg/utils/automation/prepConfig.pl "${dbName}" \
  "${hgCentralClade}" "${trackDbDir}" download/${asmId}_assembly_report.txt \
       > ${dbName}.config.ra

export taxId=`grep "^taxId" ${dbName}.config.ra | awk '{print $NF}'`
export asmDate=`grep "^assemblyDate" ${dbName}.config.ra | sed -e "s/assemblyDate \+//"`
export asmName=`grep "^ncbiAssemblyName" ${dbName}.config.ra | sed -e "s/ncbiAssemblyName \+//"`
export comName=`grep "^commonName" ${dbName}.config.ra | sed -e "s/commonName \+//"`
export sciName=`grep "^scientificName" ${dbName}.config.ra | sed -e "s/scientificName \+//"`
export orderKey=`grep "^orderKey" ${dbName}.config.ra | sed -e "s/orderKey \+//"`
export accessionID=`grep "^genBankAccessionID" ${dbName}.config.ra | sed -e "s/genBankAccessionID \+//"`
export defaultPos=`head -1 $dbName.chrom.sizes | awk '{end=int($2/2)+9999; if (end > $2){end = $2}; printf "%s:%d-%d", $1, int($2/2), end}'`

printf "DELETE from dbDb where name = \"%s\";\n" "${dbName}" > dbDbInsert.sql
printf "INSERT INTO dbDb
    (name, description, nibPath, organism,
     defaultPos, active, orderKey, genome, scientificName,
     htmlPath, hgNearOk, hgPbOk, sourceName, taxId)
VALUES\n" >> dbDbInsert.sql
printf "(\"%s\", \"%s (%s/%s)\", \"hub:/gbdb/%s/hubs\", \"%s\",
   \"%s\", 1, %d, \"%s\", \"%s\", \"/gbdb/%s/html/description.html\", 0,
     1, \"%s\", %d);\n" "${dbName}" "${asmDate}" "${asmName}" "${dbName}" "${dbName}" "${comName}" "${defaultPos}" "${orderKey}" "${comName}" "${sciName}" "${dbName}" "${accessionID}" "${taxId}" >> dbDbInsert.sql

fi

printf "# dbDbInsert.sql statement is completed:\n" 1>&2
printf "# to add to hgcentraltest: hgsql hgcentraltest < dbDbInsert.sql\n" 1>&2

if [ ! -s "chrom.sizes" ]; then
  ln -s $dbName.chrom.sizes chrom.sizes
fi
if [ ! -d "bed" ]; then
  ln -s trackData bed
fi

# establish symLinks to make this browser appear
cd "${buildDir}"
mkdir -p hubs /gbdb/$dbName
for F in "$dbName.2bit" bbi ixIxx hubs html
do
  rm -f /gbdb/$dbName/${F}
  ln -s `pwd`/${F} /gbdb/$dbName/${F}
done
rm -f hubs/chromAlias.bb hubs/chromSizes.txt hubs/groups.txt
ln -s `pwd`/trackData/chromAlias/mpxv2022.chromAlias.bb hubs/chromAlias.bb
ln -s `pwd`/mpxv2022.chrom.sizes hubs/chromSizes.txt
cp -p ~/kent/src/hg/makeDb/doc/asmHubs/groups.txt hubs/groups.txt

if [ -s "$dbName.trans.gfidx" ]; then
   printf "# sending files for dynamic blat\n#\t$dbName.2bit $dbName.trans.gfidx $dbName.untrans.gfidx\n" 1>&2
   rsync -a ./$dbName.2bit ./$dbName.trans.gfidx ./$dbName.untrans.gfidx \
      qateam@dynablat-01:/scratch/hubs/$dbName/
fi

export blatServerPresent=`hgsql -N -e 'select * from blatServers where db="'$dbName'";' hgcentraltest | wc -l`
if [ "$blatServerPresent" != 2 ]; then
  printf "# adding blatServers entry\n" 1>&2
  hgsql -e 'delete from blatServers where db="'$dbName'";' hgcentraltest
  hgsql -e 'INSERT INTO blatServers (db, host, port, isTrans, canPcr, dynamic) \
        VALUES ("'$dbName'", "dynablat-01", "4040", "1", "0", "1"); \
        INSERT INTO blatServers (db, host, port, isTrans, canPcr, dynamic) \
        VALUES ("'$dbName'", "dynablat-01", "4040", "0", "1", "1");' \
            hgcentraltest
fi
printf "# blatServers entry:\n" 1>&2
hgsql -e 'select * from blatServers where db="'$dbName'";' hgcentraltest 1>&2
