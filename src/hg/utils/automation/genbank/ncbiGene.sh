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
          | genePredFilter stdin "${accessionAsmName}.ncbiGene.ncbi.genePred"
    genePredCheck ${accessionAsmName}.ncbiGene.ncbi.genePred
    ${inside}/scripts/gpToIx.pl ${accessionAsmName}.ncbiGene.ncbi.genePred \
      | sort -u > ncbiGene.ix.txt
    ixIxx ncbiGene.ix.txt ${accessionAsmName}.ncbiGene.ix \
	${accessionAsmName}.ncbiGene.ixx
    rm -f ncbiGene.ix.txt
    genePredToBigGenePred ${accessionAsmName}.ncbiGene.ncbi.genePred stdout \
      | sort -k1,1 -k2,2n > ${accessionAsmName}.bed
    # only need to index 'name' when have the ix and ixx indexes
    bedToBigBed -type=bed12+8 -tab -as=$HOME/kent/src/hg/lib/bigGenePred.as \
      -extraIndex=name ${accessionAsmName}.bed \
        ${accessionAsmName}.ncbi.chrom.sizes \
          bbi/${accessionAsmName}.ncbiGene.ncbi.bb
    gzip -f "${accessionAsmName}.ncbiGene.ncbi.genePred"
    rm -f "${accessionAsmName}.bed"
    touch -r "${gffFile}" bbi/${accessionAsmName}.ncbiGene.ncbi.bb
    bigBedInfo bbi/${accessionAsmName}.ncbiGene.ncbi.bb \
      | egrep "^itemCount:|^basesCovered:" | sed -e 's/,//g' \
         > ${accessionAsmName}.ncbiGene.ncbi.stats.txt
    LC_NUMERIC=en_US /usr/bin/printf "# %s ncbiGene %s %'d %s %'d\n" "${dateStamp}" `cat ${accessionAsmName}.ncbiGene.ncbi.stats.txt | xargs echo`
  fi
fi

exit $?
