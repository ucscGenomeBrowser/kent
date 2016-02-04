#!/usr/bin/env bash

#    63 plant
#    71 protozoa
#    83 invertebrate
#    86 vertebrate_other
#    94 vertebrate_mammalian
#   174 fungi
#   449 archaea
#  4918 viral
# 44701 bacteria

set -beEu -o pipefail

function usage() {
  printf "usage: mkGenomesTxt.sh [ncbi|ucsc] [viral|bacteria] [refseq|genbank]\n" 1>&2
  printf "select to construct ucsc or ncbi genomes.[ucsc|ncbi].txt files\n" 1>&2
  printf "select viral or bacteria hierarchies to partition\n" 1>&2
  printf "and either in the refseq or genbank hierarchy\n" 1>&2
}

if [ $# -ne 3 ]; then
  usage
  exit 255
fi

export ncbiUcsc=$1

export fileSuffix=""

if [ "${ncbiUcsc}" = "ncbi" ]; then
  fileSuffix=".ncbi"
fi

export subType=$2

export refseqGenbank=$3

if [ "${subType}" != "viral" -a "${subType}" != "bacteria" ]; then
  printf "ERROR: select viral or bacteria assemblies update\n" 1>&2
  usage
  exit 255
fi

printf "running subType: %s\n" "${subType}" 1>&2

declare -A pageTitles

pageTitles=( ["bacteria"]="Bacterial" \
  ["viral"]="Viral" )

pageTitle="${pageTitles["$subType"]}"

export inside="/hive/data/inside/ncbi/genomes/${refseqGenbank}"
export topDir="/hive/data/inside/ncbi/genomes/${refseqGenbank}/hubs"
export dirLimit=1000

for groupName in "${subType}"
do
  cd "${topDir}"
  partsIndex="${topDir}/${groupName}/${subType}${fileSuffix}.html"
  rm -f "${partsIndex}"
  printf "# writing parts index: %s\n" "${partsIndex}" 1>&2

printf "%s\n" \
'<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
                      "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
    <meta http-equiv="Content-Type" CONTENT="text/html;CHARSET=iso-8859-1">
    <title>'$pageTitle' assembly hubs</title>
    <link rel="stylesheet" type="text/css" href="/style/HGStyle.css">
    <link rel="stylesheet" type="text/css" href="../refseq.css">
</head><body>
<!--#include virtual="/cgi-bin/hgMenubar"-->
<h1>Bacterial assembly hub</h1>
<p>
<!--#include virtual="../hubIndex'$fileSuffix'.html"-->
</p>
<h2>'$pageTitle' genome assembly hubs</h2>
<h3>Each link is a set of about 1,000 genomes in an assembly hub</h3>
<ul>' > "${partsIndex}" 

  rm -fr "/dev/shm/${groupName}"
  mkdir -p "/dev/shm/${groupName}"
  tf="/dev/shm/${groupName}/src.${groupName}.$$"
  grep "/${groupName}/" ../checksOK.list | sort -f > ${tf}
  split -l $dirLimit -d ${tf} "/dev/shm/${groupName}/${groupName}."
  for F in /dev/shm/${groupName}/${groupName}.*
do
  firstSpecies=`head -1 ${F} | tail -1 | sed -e "s#./${groupName}/##; s#/latest_assembly_versions/# #; s#/.*##;"`
  lastSpecies=`tail -1 ${F} | sed -e "s#./${groupName}/##; s#/latest_assembly_versions/# #; s#/.*##;"`
  N=`basename ${F} | sed -e "s/${groupName}.//;"`
  wrkDir="${topDir}/${groupName}/${N}"
  if [ "x${ncbiUcsc}y" = "xncbiy" ]; then
    rm -fr ${wrkDir}
    mkdir -p ${wrkDir}
  fi

  printf "<li><b>%s</b>&nbsp;<a href=%s/%s.%s%s.html target=_blank>%s to %s</a></li>\n" "$N" "$N" "${groupName}" "${N}" "$fileSuffix" "$firstSpecies" "$lastSpecies" >> "${partsIndex}"
  printf "%s working\n" "${groupName}/${N}/" 1>&2

  rm -f ${wrkDir}/groups.txt
  ln -s ../../groups.txt ${wrkDir}/groups.txt

# XXX restore this when new order${fileSuffix}.tab files are needed
# XXX if [ 0 -eq 1 ]; then
  cat ${F} | sed -e 's#^./##;' | \
while read F
do
  dirName=`dirname ${F}`
  srcDir="${inside}/${dirName}"
  B=`basename ${F} | sed -e 's/.checkAgpStatusOK.txt//;'`
  nameDefs="${srcDir}/${B}.names.tab"
  trackDb="${srcDir}/${B}.trackDb${fileSuffix}.txt"
  chromSizes="${srcDir}/${B}${fileSuffix}.chrom.sizes"
  if [ -s "${trackDb}" -a -s "${nameDefs}" ]; then
    orgName=`grep -v "^#" "${nameDefs}" | cut -f2`
    sciName=`grep -v "^#" "${nameDefs}" | cut -f5`
    asmDate=`grep -v "^#" "${nameDefs}" | cut -f9`
    asmName=`grep -v "^#" "${nameDefs}" | cut -f4`
    if [ "${asmDate}" = "(n/a)" ]; then
       continue
    fi
    # fields 1 2 3 day month year
    printf "%s" "${asmDate}" | tr '[ ]' '[\t]'
    # fields 4 5 6 7
    printf "\t%s" "${orgName}" "${sciName}" "${asmName}" "${F}"
    printf "\n"
    grep -v "^#" "${srcDir}/${B}.build.stats.txt" >> ${wrkDir}/${groupName}.${N}.total.stats${fileSuffix}.txt
  fi
done > ${wrkDir}/${groupName}.${N}.order${fileSuffix}.tab
# processing one set list into order${fileSuffix}.tab


directoryCount=`cat "${F}" | wc -l`
totalContigs=`cut -f1 ${wrkDir}/${groupName}.${N}.total.stats${fileSuffix}.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${wrkDir}/${groupName}.${N}.total.stats${fileSuffix}.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
orgCount=`cut -f4 ${wrkDir}/${groupName}.${N}.order${fileSuffix}.tab | sort -u | wc -l`
asmCount=`cut -f6 ${wrkDir}/${groupName}.${N}.order${fileSuffix}.tab | sort -u | wc -l`
orgAsmCount=`cut -f4,6 ${wrkDir}/${groupName}.${N}.order${fileSuffix}.tab | sort -u | wc -l`
duplicates=`echo "${directoryCount}" "${asmCount}" | awk '{printf "%d", $1-$2}'`
printf "%s: assemblies: %s, genomes: %s, duplicates: %s\n" \
   "$groupName.${N}" "$asmCount" "$orgCount" "$duplicates" 1>&2

rm -f "${wrkDir}/hub${fileSuffix}.txt"
printf "hub %s\n" "${groupName}.${N}" > "${wrkDir}/hub${fileSuffix}.txt"
printf "shortLabel RefSeq '%s'\n" "${groupName}.${N}" >> "${wrkDir}/hub${fileSuffix}.txt"
printf "longLabel %s RefSeq genomes, category '%s'\n" "${groupName}.${N}" "${pageTitle}" >> "${wrkDir}/hub${fileSuffix}.txt"
printf "genomesFile genomes${fileSuffix}.txt\n" >> "${wrkDir}/hub${fileSuffix}.txt"
printf "email genome-www@soe.ucsc.edu\n" >> "${wrkDir}/hub${fileSuffix}.txt"
printf "descriptionUrl %s.html\n" "${groupName}.${N}" >> "${wrkDir}/hub${fileSuffix}.txt"


export genomesTxtFile="${wrkDir}/genomes${fileSuffix}.txt"
rm -f "${genomesTxtFile}"
printf "# writing to %s\n" "${genomesTxtFile}" 1>&2

declare -A alreadyDone

# associative array alreadyDone => key is the accessName_assemblyName,
#  value is 1 -> to eliminate duplicate genomes

# sort -t'	' -k6,6 -u ${wrkDir}/${groupName}.${N}.order${fileSuffix}.tab
sort -t'	' -k6,6  ${wrkDir}/${groupName}.${N}.order${fileSuffix}.tab \
  | sort  -t'	' -k4,4f -k3,3nr -k2,2Mr -k1,1nr | cut -f7 | while read F
do
  dirName=`dirname ${F}`
  srcDir="${inside}/${dirName}"
  B=`basename ${F} | sed -e 's/.checkAgpStatusOK.txt//;'`
  if [ "${alreadyDone[$B]:-0}" = "1" ]; then
    printf "# duplicate %s\n" "$B" 1>&2
    continue
  fi
  alreadyDone[$B]="1";
  nameDefs="${srcDir}/${B}.names.tab"
  trackDb="${srcDir}/${B}.trackDb${fileSuffix}.txt"
  chromSizes="${srcDir}/${B}.${ncbiUcsc}.chrom.sizes"
# printf "# checking for %s:\n# %s\n# %s\n" "${B}" "${srcDir}" "${trackDb}" "${chromSizes}" "${nameDefs}" 1>&2

  if [ -d "${srcDir}" -a -s "${trackDb}" -a -s "${chromSizes}" -a -s "${nameDefs}" ]; then
    defaultChr=`head -1 "${chromSizes}" | cut -f1`
    chrName=`head -1 "${chromSizes}" | cut -f1`
    chrSize=`head -1 "${chromSizes}" | cut -f2`
    halfWidth=`echo $chrSize | awk '{oneTenth=$1/10; if (oneTenth > 1000000) { print "1000000"; } else {printf "%d", oneTenth;}}'`
    defaultPos=`echo $chrName $chrSize $halfWidth | awk '{printf "%s:%d-%d", $1, ($2/2 - $3), ($2/2 + $3)}'`
    orgName=`grep -v "^#" "${nameDefs}" | cut -f2`
    sciName=`grep -v "^#" "${nameDefs}" | cut -f5`
    asmDate=`grep -v "^#" "${nameDefs}" | cut -f9`
    asmName=`grep -v "^#" "${nameDefs}" | cut -f4`
    printf "\ngenome %s\n" "$B" >> "${genomesTxtFile}"
    printf "description %s/%s\n" "${asmDate}" "${asmName}" >> "${genomesTxtFile}"
    printf "trackDb %s/%s.trackDb%s.txt\n" "$B" "${B}" "$fileSuffix" >> "${genomesTxtFile}"
    printf "groups groups.txt\n" >> "${genomesTxtFile}"
    printf "twoBitPath %s/%s.%s.2bit\n" "${B}" "${B}" "$ncbiUcsc" >> "${genomesTxtFile}"
    printf "organism %s\n" "${orgName}" >> "${genomesTxtFile}"
    printf "scientificName %s\n" "${sciName}" >> "${genomesTxtFile}"
    printf "defaultPos %s\n" "${defaultPos}" >> "${genomesTxtFile}"
    printf "htmlPath %s/%s.description.html\n" "${B}" "${B}" >> "${genomesTxtFile}"
    rm -f "${wrkDir}/${B}"
    ln -s "${srcDir}" "${wrkDir}/${B}"
#    printf "%s\n" "${B}" 1>&2
  fi
done # processing one order${fileSuffix}.tab into a genomes${fileSuffix}.txt file

# XXX fi # temporarily off

done # processing one set list

  printf "%s\n" '</ul>
<!--#include virtual="'${subType}'StatsIndexTable.'${ncbiUcsc}'.html"-->
<script type="text/javascript" src="../../sorttable.js"></script>
</body></html>' >> "${partsIndex}"
  chmod +x "${partsIndex}"

done # processing: for groupName in ${subType}

cd "${topDir}"

cat ${subType}/??/*.order${fileSuffix}.tab > ${subType}.order${fileSuffix}.tab
cat ${subType}/??/*.total.stats${fileSuffix}.txt > ${subType}/${subType}.total.stats${fileSuffix}.txt

exit $?
