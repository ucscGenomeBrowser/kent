#!/bin/sh

#      2 other
#    112 vertebrate_other
#    132 vertebrate_mammalian
#    170 plant
#    183 protozoa
#    275 invertebrate
#    474 archaea
#    941 fungi
#  32804 bacteria

export inside="/hive/data/inside/ncbi/genomes/genbank"
export topDir="/hive/data/inside/ncbi/genomes/genbank/hubs"
export dirLimit=1000

for groupName in bacteria
do
  cd "${topDir}"
  partsIndex="${topDir}/${groupName}/bacteria.ncbi.html"
  rm -f "${partsIndex}"

printf "%s\n" \
'<html><head>
    <meta http-equiv="Content-Type" CONTENT="text/html;CHARSET=iso-8859-1">
    <title>Bacterial assembly hubs</title>
    <link rel="STYLESHEET" href="/style/HGStyle.css">
</head><body>
<!--#include virtual="/cgi-bin/hgMenubar"-->
<h1>Bacterial assembly hub</h1>
<p>
<!--#include virtual="../hubIndex.ncbi.html"-->
</p>
<h2>bacterial genome assembly hubs</h2>
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
  mkdir -p ${wrkDir}

  printf "<li><a href=%s/%s.%s.html target=_blank>%s to %s</a></li>\n" "$N" "${groupName}" "${N}" "$firstSpecies" "$lastSpecies" >> "${partsIndex}"
  printf "%s\n" "$N" 1>&2

  rm -f ${wrkDir}/groups.txt
  ln -s ../../groups.txt ${wrkDir}/groups.txt

# XXX restore this when new order.tab files are needed
# XXX if [ 0 -eq 1 ]; then
  cat ${F} | sed -e 's#^./##;' | \
while read F
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
    printf "%s" "${asmDate}" | tr '[ ]' '[\t]'
    # fields 4 5 6 7
    printf "\t%s" "${orgName}" "${sciName}" "${asmName}" "${F}"
    printf "\n"
    grep -v "^#" "${srcDir}/${B}.build.stats.txt" >> ${wrkDir}/${groupName}.${N}.total.stats.txt
  fi
done > ${wrkDir}/${groupName}.${N}.order.tab
# processing one set list into order.tab


directoryCount=`cat "${F}" | wc -l`
totalContigs=`cut -f1 ${wrkDir}/${groupName}.${N}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
totalSize=`cut -f2 ${wrkDir}/${groupName}.${N}.total.stats.txt | ave stdin | grep -w total | awk '{print $NF}' | sed -e 's/.000000//'`
orgCount=`cut -f4 ${wrkDir}/${groupName}.${N}.order.tab | sort -u | wc -l`
asmCount=`cut -f6 ${wrkDir}/${groupName}.${N}.order.tab | sort -u | wc -l`
orgAsmCount=`cut -f4,6 ${wrkDir}/${groupName}.${N}.order.tab | sort -u | wc -l`
duplicates=`echo "${directoryCount}" "${asmCount}" | awk '{printf "%d", $1-$2}'`
printf "%s: assemblies: %s, genomes: %s, duplicates: %s\n" \
   "$groupName.${N}" "$asmCount" "$orgCount" "$duplicates" 1>&2

rm -f "${wrkDir}/hub.ncbi.txt"
printf "hub %s\n" "${groupName}.${N}" > "${wrkDir}/hub.ncbi.txt"
printf "shortLabel Genbank '%s'\n" "${groupName}.${N}" >> "${wrkDir}/hub.ncbi.txt"
printf "longLabel Synthetic genomes from Genbank, category '%s'\n" "${groupName}.${N}" >> "${wrkDir}/hub.ncbi.txt"
printf "genomesFile genomes.ncbi.txt\n" >> "${wrkDir}/hub.ncbi.txt"
printf "email genome-www@soe.ucsc.edu\n" >> "${wrkDir}/hub.ncbi.txt"
printf "descriptionUrl %s.html\n" "${groupName}.${N}" >> "${wrkDir}/hub.ncbi.txt"

rm -f "${wrkDir}/genomes.ncbi.txt"

sort -t'	' -k6,6 -u ${wrkDir}/${groupName}.${N}.order.tab \
  | sort  -t'	' -k4,4f -k3,3nr -k2,2Mr -k1,1nr | cut -f7 | while read F
do
  dirName=`dirname ${F}`
  srcDir="${inside}/${dirName}"
  B=`basename ${F} | sed -e 's/.checkAgpStatusOK.txt//;'`
  nameDefs="${srcDir}/${B}.names.tab"
  trackDb="${srcDir}/${B}.trackDb.ncbi.txt"
  chromSizes="${srcDir}/${B}.ncbi.chrom.sizes"
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
    printf "\ngenome %s\n" "$B" >> "${wrkDir}/genomes.ncbi.txt"
    printf "description %s/%s\n" "${asmDate}" "${asmName}" >> "${wrkDir}/genomes.ncbi.txt"
    printf "trackDb %s/%s.trackDb.ncbi.txt\n" "$B" "${B}" >> "${wrkDir}/genomes.ncbi.txt"
    printf "groups groups.txt\n" >> "${wrkDir}/genomes.ncbi.txt"
    printf "twoBitPath %s/%s.ncbi.2bit\n" "${B}" "${B}" >> "${wrkDir}/genomes.ncbi.txt"
    printf "organism %s\n" "${orgName}" >> "${wrkDir}/genomes.ncbi.txt"
    printf "scientificName %s\n" "${sciName}" >> "${wrkDir}/genomes.ncbi.txt"
    printf "defaultPos %s\n" "${defaultPos}" >> "${wrkDir}/genomes.ncbi.txt"
    printf "htmlPath %s/%s.description.html\n" "${B}" "${B}" >> "${wrkDir}/genomes.ncbi.txt"
    rm -f "${wrkDir}/${B}"
    ln -s "${srcDir}" "${wrkDir}/${B}"
    printf "%s\n" "${B}" 1>&2
#    ./hubHtml.pl "${srcDir}/${B}" >> "${groupName}/${groupName}.html"
  fi
done # processing one order.tab into a genomes.ncbi.txt file

# XXX fi # temporarily off

done # processing one set list

  printf "%s\n" '</ul></body></html>' >>"${partsIndex}"

done # processing: for groupName in bacteria

exit $?
# ./bacteria/Pasteurella_multocida/latest_assembly_versions/GCA_000219315.2_ASM21931v2/GCA_000219315.2_ASM21931v2.checkAgpStatusOK.txt

