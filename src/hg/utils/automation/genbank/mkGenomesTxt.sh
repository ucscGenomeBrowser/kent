#!/usr/bin/env bash

set -beEu -o pipefail

#    63 plant
#    71 protozoa
#    83 invertebrate
#    86 vertebrate_other
#    94 vertebrate_mammalian
#   174 fungi
#   449 archaea
#  4918 viral
# 44701 bacteria

if [ $# -ne 2 ]; then
  printf "usage: mkGenomesTxt.sh [ncbi|ucsc] [refseq|genbank]\n" 1>&2
  printf "select to construct ucsc or ncbi genomes.txt files\n" 1>&2
  printf "in either the refseq or genbank hierarchy\n" 1>&2
  exit 255
fi

export ncbiUcsc=$1
export refseqGenbank=$2

export fileSuffix=""

if [ "${ncbiUcsc}" = "ncbi" ]; then
  fileSuffix=".ncbi"
fi

export inside="/hive/data/inside/ncbi/genomes/${refseqGenbank}"
export topDir="/hive/data/inside/ncbi/genomes/${refseqGenbank}/hubs"

export genbankList="other vertebrate_other vertebrate_mammalian plant protozoa invertebrate archaea"

# for groupName in plant protozoa invertebrate vertebrate_other vertebrate_mammalian fungi archaea
for groupName in ${genbankList}
do
cd "${topDir}"
directoryCount=`grep "/${groupName}/" ../checksOK.list | wc -l`
printf "# count in %s: %'d\n" "${groupName}" "${directoryCount}" 1>&2
mkdir -p "${groupName}"

rm -f ${groupName}/${groupName}.total.stats.txt

grep "/${groupName}/" ../checksOK.list | sed -e 's#^./##;' \
  | sed -e '/^$/d;' | while read F
do
  dirName=`dirname ${F}`
  srcDir="${inside}/${dirName}"
  B=`basename ${F} | sed -e 's/.checkAgpStatusOK.txt//;'`
  nameDefs="${srcDir}/${B}.names.tab"
  trackDb="${srcDir}/${B}.trackDb${fileSuffix}.txt"
  chromSizes="${srcDir}/${B}.${ncbiUcsc}.chrom.sizes"
  if [ -s "${trackDb}" ]; then
    orgName=`grep -v "^#" "${nameDefs}" | cut -f2`
    sciName=`grep -v "^#" "${nameDefs}" | cut -f5`
    asmDate=`grep -v "^#" "${nameDefs}" | cut -f9`
    asmName=`grep -v "^#" "${nameDefs}" | cut -f4`
    # fields 1 2 3 day month year
    echo -n -e "${asmDate}" | tr '[ ]' '[\t]'
    # fields 4 5 6 7
    printf "\t%s\t%s\t%s\t%s\n"  "${orgName}" "${sciName}" "${asmName}" "${F}"
#     echo -e "\t${orgName}\t${sciName}\t${asmName}\t${F}"
    grep -v "^#" "${srcDir}/${B}.build.stats.txt" >> ${groupName}/${groupName}.total.stats.txt
  fi
done > ${groupName}.order.tab

rm -f "${groupName}.order.ncbi.tab"
ln -s "${groupName}.order.tab" "${groupName}.order.ncbi.tab"

totalContigs=`cut -f1 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${groupName}/${groupName}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
orgCount=`cut -f4 ${groupName}.order.tab | sort -u | wc -l`
asmCount=`cut -f4,6 ${groupName}.order.tab | sort -u | wc -l`
orgAsmCount=`cut -f4,6 ${groupName}.order.tab | sort -u | wc -l`
duplicates=`echo "${directoryCount}" "${asmCount}" | awk '{printf "%d", $1-$2}'`
LC_NUMERIC=en_US /usr/bin/printf "# %s genomes: %'d, assemblies: %'d, duplicates: %'d\n" "${groupName}" "${orgCount}" "${asmCount}" "${duplicates}" 1>&2

# XXX temporary break in the action here, just rebuilding the html files
## continue

# the -k6,6 -u sort eliminates duplicate assemblies
# the order sort is by orgName then year then month then day, newest first

declare -A alreadyDone

# associative array alreadyDone => key is the accessName_assemblyName,
#  value is 1 -> to eliminate duplicate genomes

# sort -t'	' -k6,6 -u ${groupName}.order.tab
sort -t'	' -k6,6 ${groupName}.order.tab \
  | sort  -t'	' -k4,4f -k3,3nr -k2,2Mr -k1,1nr | cut -f7 | sed -e '/^$/d;' \
| while read F
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
  if [ -s "${trackDb}" ]; then
    defaultChr=`head -1 "${chromSizes}" | cut -f1`
    chrName=`head -1 "${chromSizes}" | cut -f1`
    chrSize=`head -1 "${chromSizes}" | cut -f2`
    halfWidth=`echo $chrSize | awk '{oneTenth=$1/10; if (oneTenth > 1000000) { print "1000000"; } else {printf "%d", oneTenth;}}'`
    defaultPos=`echo $chrName $chrSize $halfWidth | awk '{printf "%s:%d-%d", $1, ($2/2 - $3), ($2/2 + $3)}'`
    orgName=`grep -v "^#" "${nameDefs}" | cut -f2`
    sciName=`grep -v "^#" "${nameDefs}" | cut -f5`
    asmDate=`grep -v "^#" "${nameDefs}" | cut -f9`
    asmName=`grep -v "^#" "${nameDefs}" | cut -f4`
    rm -f "${topDir}/${groupName}/${B}"
    ln -s "${srcDir}" "${topDir}/${groupName}/${B}"
    printf "%s\n" "genome $B
description ${asmDate}/${asmName}
trackDb $B/${B}.trackDb${fileSuffix}.txt
groups groups.txt
twoBitPath ${B}/${B}.${ncbiUcsc}.2bit
organism ${orgName}
scientificName ${sciName}
defaultPos ${defaultPos}
htmlPath ${B}/${B}.description.html" > "${topDir}/${groupName}/${B}/${B}.genomes${fileSuffix}.txt"
    printf "\n"
    cat "${topDir}/${groupName}/${B}/${B}.genomes${fileSuffix}.txt"
  fi
done > "${groupName}/genomes${fileSuffix}.txt"


done

exit $?
GCA_000164805.2_Tarsius_syrichta-2.0.1.ucsc.chrom.sizes
GCA_000164805.2_Tarsius_syrichta-2.0.1.names.tab

#taxId  commonName      submitter       asmName orgName bioSample       asmTypeasmLevel asmDate asmAccession
9478    Tarsius syrichta        Washington University (WashU)   Tarsius_syrichta-2.0.1  Tarsius syrichta        SAMN02445010    haploid Scaffold        18 Sep 2013     GCA_000164805.2


vertebrate_mammalian/Odobenus_rosmarus/latest_assembly_versions/GCA_000321225.1_Oros_1.0/GCA_000321225.1_Oros_1.0.checkAgpStatusOK.txt
vertebrate_mammalian/Jaculus_jaculus/latest_assembly_versions/GCA_000280705.1_JacJac1.0/GCA_000280705.1_JacJac1.0.checkAgpStatusOK.txt
vertebrate_mammalian/Tarsius_syrichta/latest_assembly_versions/GCA_000164805.2_Tarsius_syrichta-2.0.1/GCA_000164805.2_Tarsius_syrichta-2.0.1.checkAgpStatusOK.txt

genome GCA_000004335.1_AilMel_1.0
trackDb GCA_000004335.1_AilMel_1.0/GCA_000004335.1_AilMel_1.0.trackDb.txt
groups groups.txt
description 15 Dec 2009/AilMel_1.0
twoBitPath GCA_000004335.1_AilMel_1.0/GCA_000004335.1_AilMel_1.0.ucsc.2bit
organism Ailuropoda melanoleuca
defaultPos GL192338v1:1000000-2000000
orderKey 4700
scientificName Ailuropoda melanoleuca
