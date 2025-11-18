#!/bin/bash

# exit on any error
set -beEu -o pipefail

if [ $# -ne 2 ]; then
 printf "usage: faToTwoBit.sh /path/to/assembly.fa /destination/build/directory/
    will process the assembly.fa file into a set of files to use
    as an assembly hub for the UCSC Genome Browser.
Expected that the /destination/build/directory/ already exists.
The 'name' of the assembly hub will be the 'assembly' name from
    the 'assembly.fa' file name.
The assembly.fa file can be gzipped specified with the .gz extension:
    assembly.fa.gz\n" 1>&2
  exit 255
fi

#### expected two arguments ####
export asmFa="${1}"
export destDir="${2}"

#### must have destDir existing ####
if [ ! -d "${destDir}" ]; then
  printf "expecting the destination directory to already exist\n" 1>&2
  printf "do not see '%s' as a directory\n" "${destDir}" 1>&2
  exit 255
fi

#### establish fundamental variables ####
export buildLog="${destDir}/build.log"
export asmFile=`basename ${asmFa}`
export noGz="${asmFile%.gz}"
export asmId="${noGz%.*}"

#### working in destDir ####
cd "${destDir}"
mkdir -p html bbi ixIxx

#### record progress in buildLog ####
printf "#### %s ####\n"  "`date \"+%F %T\"`" >> $buildLog
printf "# processing %s into assembly %s\n" "${asmFile}" "${asmId}" 1>&2
printf "# processing %s into assembly %s\n" "${asmFile}" "${asmId}" >> $buildLog

#### construct 2bit and 2bit.bpt from assembly.fa sequence ####
if [ "${asmFa}" -nt "${asmId}.2bit" ]; then
   printf "# time faToTwoBit ${asmFa} ${asmId}.2bit\n" 1>&2
   printf "# time faToTwoBit ${asmFa} ${asmId}.2bit\n" >> $buildLog
   time faToTwoBit "${asmFa}" "${asmId}.2bit" >> $buildLog 2>&1
   printf "# time bptForTwoBit ${asmId}.2bit ${asmId}.2bit.bpt\n" >> $buildLog
   time bptForTwoBit "${asmId}.2bit" "${asmId}.2bit.bpt" >> $buildLog 2>&1
   touch -r "${asmFa}" "${asmId}.2bit"
   touch -r "${asmFa}" "${asmId}.2bit.bpt"
fi

#### construct chrom.sizes.txt from 2bit file ####
if [ "${asmId}.2bit" -nt "${asmId}.chrom.sizes.txt" ]; then
   printf "# time twoBitInfo ${asmId}.2bit stdout | sort -k2,2nr > ${asmId}.chrom.sizes.txt\n" 1>&2
   printf "# time twoBitInfo ${asmId}.2bit stdout | sort -k2,2nr > ${asmId}.chrom.sizes.txt\n" >> $buildLog
   time twoBitInfo "${asmId}.2bit" stdout | sort -k2,2nr > "${asmId}.chrom.sizes.txt" 2>> $buildLog
   touch -r "${asmId}.2bit" "${asmId}.chrom.sizes.txt"
fi

#### construct AGP file from assembly.fa sequence ####
if [ "${asmFa}" -nt "${asmId}.agp.gz" ]; then
  printf "# time hgFakeAgp -minContigGap=1 -minScaffoldGap=1000 -singleContigs  "${asmFa}" stdout | gzip -c > ${asmId}.agp.gz\n" 1>&2
  printf "# time hgFakeAgp -minContigGap=1 -minScaffoldGap=1000 -singleContigs  "${asmFa}" stdout | gzip -c > ${asmId}.agp.gz\n" >> $buildLog
  hgFakeAgp -minContigGap=1 -minScaffoldGap=1000 -singleContigs  "${asmFa}" stdout | gzip -c > "${asmId}.agp.gz" 2>> $buildLog
   touch -r "${asmFa}" "${asmId}.agp.gz"
fi

wget --timestamping "https://github.com/ucscGenomeBrowser/kent/raw/master/src/hg/makeDb/doc/asmHubs/groups.txt"

# calculate a default position in the middle of the largest sequence
export chrName=`head -1 $asmId.chrom.sizes.txt | cut -f1`
export bigChrom=`head -1 $asmId.chrom.sizes.txt | awk '{print $NF}'`
export oneThird=`awk -v s=$bigChrom 'BEGIN {printf "%d", s/3}'`
export tenK=`awk -v t=$oneThird -v m=$bigChrom 'BEGIN {r=t + 10000; if (r > m) { r = m}; printf "%d", r}'`
export defPos="${chrName}:${oneThird}-${tenK}"

printf "hub $asmId genome assembly
shortLabel $asmId
longLabel $asmId
useOneFile on
email yourEmail@uni.edu
descriptionUrl html/$asmId.description.html

genome $asmId
groups groups.txt
description $asmId
twoBitPath $asmId.2bit
twoBitBptUrl $asmId.2bit.bpt
organism $asmId
defaultPos $defPos
scientificName $asmId
htmlPath html/$asmId.description.html
" > ${destDir}/hub.txt

printf "<h1>assembly description page</h1>
<p>
Should be filled in with information about this assembly hub.
</p>\n" > ${destDir}/html/$asmId.description.html

###### assembly/gap ####################
mkdir -p "${destDir}/trackData/assemblyGap"
cd "${destDir}/trackData/assemblyGap"
if [ "../../${asmId}.agp.gz" -nt "${asmId}.assembly.bb" ]; then
  printf "# constructing assembly/gap tracks\n" 1>&2
  zgrep -v "^#" ../../$asmId.agp.gz | awk '$5 != "N" && $5 != "U"' \
     | awk '{printf "%s\t%d\t%d\t%s\t0\t%s\n", $1, $2-1, $3, $6, $9}' \
        | sort -k1,1 -k2,2n > $asmId.assembly.bed
  zgrep -v "^#" ../../$asmId.agp.gz | awk '$5 == "N" || $5 == "U"' \
     | awk '{printf "%s\t%d\t%d\t%s\n", $1, $2-1, $3, $8}' \
        | sort -k1,1 -k2,2n > $asmId.gap.bed

  bedToBigBed -extraIndex=name -verbose=0 $asmId.assembly.bed \
    ../../$asmId.chrom.sizes.txt $asmId.assembly.bb
  touch -r ../../$asmId.agp.gz $asmId.assembly.bb
  $HOME/kent/src/hg/utils/automation/genbank/nameToIx.pl \
    $asmId.assembly.bed | sort -u > $asmId.assembly.ix.txt
  if [ -s $asmId.assembly.ix.txt ]; then
    ixIxx $asmId.assembly.ix.txt $asmId.assembly.ix $asmId.assembly.ixx
  fi
  if [ -s "${asmId}.gap.bed" ]; then
    bedToBigBed -extraIndex=name -verbose=0 $asmId.gap.bed \
      ../../$asmId.chrom.sizes.txt $asmId.gap.bb
    touch -r ../../$asmId.agp.gz $asmId.gap.bb
    rm -f ${destDir}/bbi/$asmId.gap.bb
    ln -s ../trackData/assemblyGap/$asmId.gap.bb ${destDir}/bbi/$asmId.gap.bb
  else
    rm -f "${asmId}.gap.bed"
  fi
else
  printf "# assembly/gap previously completed\n" 1>&2
fi

cd "${destDir}"

if [ -s "${destDir}/trackData/assemblyGap/$asmId.assembly.bb" ]; then
  rm -f ${destDir}/bbi/$asmId.assembly.bb
  ln -s ../trackData/assemblyGap/$asmId.assembly.bb ${destDir}/bbi/$asmId.assembly.bb
  if [ -s "${destDir}/trackData/assemblyGap/$asmId.assembly.ix" ]; then
    rm -f ${destDir}/ixIxx/$asmId.assembly.ix ${destDir}/ixIxx/$asmId.assembly.ixx
    ln -s ../trackData/assemblyGap/$asmId.assembly.ix ${destDir}/ixIxx/$asmId.assembly.ix
    ln -s ../trackData/assemblyGap/$asmId.assembly.ixx ${destDir}/ixIxx/$asmId.assembly.ixx
  fi

  printf "
track assembly
longLabel Assembly
shortLabel Assembly
visibility pack
colorByStrand 150,100,30 230,170,40
color 150,100,30
altColor 230,170,40
bigDataUrl bbi/$asmId.assembly.bb
type bigBed 6
html html/$asmId.assembly
searchIndex name
searchTrix ixIxx/$asmId.assembly.ix
url https://www.ncbi.nlm.nih.gov/nuccore/\$\$
urlLabel NCBI Nucleotide database
group map
" >> ${destDir}/hub.txt

printf "<h1>assembly track description page</h1>
<p>
Should be filled in with information about this track.
</p>\n" > ${destDir}/html/$asmId.assembly.html

fi

if [ -s "${destDir}/trackData/assemblyGap/$asmId.gap.bb" ]; then
  rm -f ${destDir}/bbi/$asmId.gap.bb
  ln -s ../trackData/assemblyGap/$asmId.gap.bb ${destDir}/bbi/$asmId.gap.bb

  printf "
track gap
longLabel AGP gap
shortLabel Gap (AGP defined)
visibility dense
color 0,0,0
bigDataUrl bbi/$asmId.gap.bb
type bigBed 4
group map
html html/$asmId.gap
" >> ${destDir}/hub.txt

printf "<h1>gap track description page</h1>
<p>
Should be filled in with information about this track.
</p>\n" > ${destDir}/html/$asmId.gap.html

fi

cd "${destDir}"

###### cytoBand ####################
mkdir -p "${destDir}/trackData/cytoBand"
cd "${destDir}/trackData/cytoBand"
if [ ../../$asmId.chrom.sizes.txt -nt $asmId.cytoBand.bb ]; then
  printf "# construction cytoBand track\n" 1>&2
  awk '{printf "%s\t0\t%d\t\tgneg\n", $1, $2}' ../../$asmId.chrom.sizes.txt | sort -k1,1 -k2,2n > $asmId.cytoBand.bed
  bedToBigBed -type=bed4+1 -as=$HOME/kent/src/hg/lib/cytoBand.as -tab $asmId.cytoBand.bed ../../$asmId.chrom.sizes.txt $asmId.cytoBand.bb >> $buildLog 2>&1

  touch -r ../../$asmId.chrom.sizes.txt $asmId.cytoBand.bb
else
  printf "# cytoBand previously completed\n" 1>&2
fi

cd "${destDir}"

if [ -s "${destDir}/trackData/cytoBand/$asmId.cytoBand.bb" ]; then
  rm -f ${destDir}/bbi/$asmId.cytoBand.bb
  ln -s ../trackData/cytoBand/$asmId.cytoBand.bb ${destDir}/bbi/$asmId.cytoBand.bb
  printf "
track cytoBandIdeo
shortLabel Chromosome Band (Ideogram)
longLabel Ideogram for Orientation
group map
visibility dense
type bigBed 4 +
bigDataUrl bbi/$asmId.cytoBand.bb
" >> ${destDir}/hub.txt

fi

###### gc5Base ####################
mkdir -p "${destDir}/trackData/gc5Base"
cd "${destDir}/trackData/gc5Base"
if [ ../../$asmId.2bit -nt $asmId.gc5Base.bw ]; then
  printf "# constructin gc5Base track\n" 1>&2
  hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \
    ../../$asmId.2bit 2>> $buildLog \
      | gzip -c > $asmId.wigVarStep.gz
  wigToBigWig $asmId.wigVarStep.gz ../../$asmId.chrom.sizes.txt $asmId.gc5Base.bw >> $buildLog 2>&1
  rm -f $asmId.wigVarStep.gz
  touch -r ../../$asmId.2bit $asmId.gc5Base.bw
else
  printf "# gc5Base previously completed\n" 1>&2
fi

cd "${destDir}"

if [ -s "${destDir}/trackData/gc5Base/$asmId.gc5Base.bw" ]; then
  rm -f ${destDir}/bbi/$asmId.gc5Base.bw
  ln -s ../trackData/gc5Base/$asmId.gc5Base.bw ${destDir}/bbi/$asmId.gc5Base.bw

  printf "
track gc5Base
shortLabel GC Percent
longLabel GC Percent in 5-Base Windows
group map
visibility full
autoScale Off
maxHeightPixels 128:36:16
graphTypeDefault Bar
gridDefault OFF
windowingFunction Mean
color 0,0,0
altColor 128,128,128
viewLimits 30:70
type bigWig 0 100
bigDataUrl bbi/$asmId.gc5Base.bw
html html/$asmId.gc5Base
" >> ${destDir}/hub.txt

printf "<h1>gc5Base description page</h1>
<p>
Should be filled in with information about this track.
</p>\n" > ${destDir}/html/$asmId.gc5Base.html

fi
