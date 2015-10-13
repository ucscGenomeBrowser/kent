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

export inside="/hive/data/inside/ncbi/genomes/refseq"
export topDir="/hive/data/inside/ncbi/genomes/refseq/hubs"

for groupName in plant protozoa invertebrate vertebrate_other vertebrate_mammalian fungi archaea
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
  trackDb="${srcDir}/${B}.trackDb.ncbi.txt"
  chromSizes="${srcDir}/${B}.ncbi.chrom.sizes"
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
  if [ "x1y" = "x${alreadyDone[${B}]}y" ]; then
    printf "# duplicate %s\n" "${B}" 1>&2
    continue
  fi
  alreadyDone[${B}]="1";
  nameDefs="${srcDir}/${B}.names.tab"
  trackDb="${srcDir}/${B}.trackDb.ncbi.txt"
  chromSizes="${srcDir}/${B}.ncbi.chrom.sizes"
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
    echo
    echo genome $B
    echo "description ${asmDate}/${asmName}"
    echo trackDb $B/${B}.trackDb.ncbi.txt
    echo groups groups.txt
    echo twoBitPath ${B}/${B}.ncbi.2bit
    echo "organism ${orgName}"
    echo "scientificName ${sciName}"
    echo "defaultPos ${defaultPos}"
    echo "htmlPath ${B}/${B}.description.html"
    rm -f "${topDir}/${groupName}/${B}"
    ln -s "${srcDir}" "${topDir}/${groupName}/${B}"
  fi
done > "${groupName}/genomes.ncbi.txt"


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
