#!/bin/bash

# ncbiGene.sh - process the *_genomic.gff.gz file into a bigGenePred file

# fail on any error:
set -beEu -o pipefail

if [ $# -ne 2 ]; then
  printf "%s\n" "usage: ncbiGene.sh <outside/pathTo/*_genomic.gff.gz> <inside/destinationDir/>" 1>&2
  exit 255
fi

export dateStamp=`date "+%FT%T %s"`

export inside="/hive/data/inside/ncbi/genomes/refseq"

export gffFile=$1
export destDir=$2
export baseFile=`basename "${gffFile}"`
export accessionAsmName=`echo $baseFile | sed -e 's/_genomic.gff.gz//;'`

# may not have a gff file, not a failure
if [ ! -s "${gffFile}" ]; then
  printf "# %s no gffFile %s\n" "${dateStamp}" "${gffFile}" 1>&2
  exit 0
fi

function cleanUp() {
  rm -f ${accessionAsmName}.ncbiGene.ncbi.genePred.gz \
    ${accessionAsmName}.ncbiGene.ncbi.genePred \
    bbi/${accessionAsmName}.ncbiGene.ncbi.bb \
    bbi/${accessionAsmName}.ncbiGene.ucsc.bb \
    ${accessionAsmName}.ncbiGene.ix* \
    ncbiGene.ix.txt \
    ${accessionAsmName}.ncbiGene.html \
    ${accessionAsmName}.geneAttrs.ncbi.txt \
    ${accessionAsmName}.ncbiGene.ucsc.genePred.gz
}

if [ -d "${destDir}" ]; then
  cd "${destDir}"
### XXX temporary force rebuild
  rm -f bbi/${accessionAsmName}.ncbiGene.ncbi.bb
  if [ "${gffFile}" -nt bbi/${accessionAsmName}.ncbiGene.ncbi.bb ]; then
    printf "# %s processing %s\n" "${dateStamp}" "${gffFile}" 1>&2
    rm -f bbi/${accessionAsmName}.ncbiGene.ncbi.bb
    (gff3ToGenePred -warnAndContinue -useName \
       -attrsOut=${accessionAsmName}.geneAttrs.ncbi.txt \
         "${gffFile}"  stdout 2> ${accessionAsmName}.ncbiGene.log.txt || true) \
          | genePredFilter stdin stdout \
           | gzip -c > "${accessionAsmName}.ncbiGene.ncbi.genePred.gz"
    genePredCheck ${accessionAsmName}.ncbiGene.ncbi.genePred.gz
    howMany=`genePredCheck ${accessionAsmName}.ncbiGene.ncbi.genePred.gz 2>&1 | grep "^checked:" | awk '{print $2}'`
    if [ "${howMany}" -eq 0 ]; then
      printf "# %s no genes found in %s\n" "${dateStamp}" "${gffFile}" 1>&2
      cleanUp
      exit 0
    fi
    printf "# %s before lifting %s\n" "${dateStamp}" "${accessionAsmName}.ncbiGene.ncbi.genePred.gz" 1>&2
    if [ -s "${accessionAsmName}.ncbiToUcsc.lift" ]; then
      printf "# %s lifting %s\n" "${dateStamp}" "${accessionAsmName}.ncbiGene.ncbi.genePred.gz" 1>&2
      liftUp -extGenePred -type=.gp stdout \
        ${accessionAsmName}.ncbiToUcsc.lift error \
       ${accessionAsmName}.ncbiGene.ncbi.genePred.gz | gzip -c \
          > ${accessionAsmName}.ncbiGene.ucsc.genePred.gz
       touch -r ${accessionAsmName}.ncbiGene.ncbi.genePred.gz \
        ${accessionAsmName}.ncbiGene.ucsc.genePred.gz
    fi
    ${inside}/scripts/gpToIx.pl ${accessionAsmName}.ncbiGene.ncbi.genePred.gz \
      | sort -u > ncbiGene.ix.txt
    ixIxx ncbiGene.ix.txt ${accessionAsmName}.ncbiGene.ix \
	${accessionAsmName}.ncbiGene.ixx
    rm -f ncbiGene.ix.txt
    printf "# %s genePredToBigGenePred %s\n" "${dateStamp}" "${accessionAsmName}.ncbiGene.ncbi.genePred.gz" 1>&2
    genePredToBigGenePred ${accessionAsmName}.ncbiGene.ncbi.genePred.gz stdout \
      | sort -k1,1 -k2,2n > ${accessionAsmName}.bed
    printf "# %s bedToBigBed %s\n" "${dateStamp}" "${accessionAsmName}.ncbiGene.ncbi.genePred.gz" 1>&2
    # only need to index 'name' when have the ix and ixx indexes
    (bedToBigBed -type=bed12+8 -tab -as=$HOME/kent/src/hg/lib/bigGenePred.as \
      -extraIndex=name ${accessionAsmName}.bed \
        ${accessionAsmName}.ncbi.chrom.sizes \
          bbi/${accessionAsmName}.ncbiGene.ncbi.bb || true)
    if [ ! -s bbi/${accessionAsmName}.ncbiGene.ncbi.bb ]; then
       printf "# %s failing bedToBigBed %s\n" "${dateStamp}" "${gffFile}" 1>&2
       cleanUp
       exit 0
    fi
    rm -f "${accessionAsmName}.bed"
    touch -r "${gffFile}" bbi/${accessionAsmName}.ncbiGene.ncbi.bb
    bigBedInfo bbi/${accessionAsmName}.ncbiGene.ncbi.bb \
      | egrep "^itemCount:|^basesCovered:" | sed -e 's/,//g' \
         > ${accessionAsmName}.ncbiGene.ncbi.stats.txt
    LC_NUMERIC=en_US /usr/bin/printf "# %s ncbiGene %s %'d %s %'d\n" "${dateStamp}" `cat ${accessionAsmName}.ncbiGene.ncbi.stats.txt | xargs echo`
    if [ -s "${accessionAsmName}.ncbiGene.ucsc.genePred.gz" ]; then
      printf "# %s building ncbiGene for UCSC browser %s\n" "${dateStamp}" "${accessionAsmName}" 1>&2
    genePredToBigGenePred ${accessionAsmName}.ncbiGene.ucsc.genePred.gz stdout \
        | sort -k1,1 -k2,2n > ${accessionAsmName}.bed
      bedToBigBed -type=bed12+8 -tab -as=$HOME/kent/src/hg/lib/bigGenePred.as \
        -extraIndex=name ${accessionAsmName}.bed \
          ${accessionAsmName}.ucsc.chrom.sizes \
            bbi/${accessionAsmName}.ncbiGene.ucsc.bb
      rm -f "${accessionAsmName}.bed"
      touch -r "${gffFile}" bbi/${accessionAsmName}.ncbiGene.ucsc.bb
    fi
  fi
fi

exit $?
