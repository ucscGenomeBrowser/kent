#!/bin/bash

set -beEu -o pipefail

export outside="/hive/data/outside/ncbi/genomes/genbank"
export inside="/hive/data/inside/ncbi/genomes/genbank"

if [ $# -lt 1 ]; then
  echo "usage: ucscToNcbi.sh <pathTo>/*_genomic.fna.gz" 1>&2
  echo "  that unrooted <pathTo> is to a file in .../all_assembly_versions/..." 1>&2
  echo "  relative to the directory hierarchy: $outside" 1>&2
  echo "expecting to find corresponding files in the same directory path" 1>&2
  echo "  with .../latest_assembly_versions/... instead of /all_.../" 1>&2
  echo "results will be constructing files in corresponding directory" 1>&2
  echo "  relative to the directory hierarchy: $inside" 1>&2
  exit 255
fi

export fnaFile=$1

B=`basename "${fnaFile}" | sed -e 's/_genomic.fna.gz//;'`
D=`dirname "${fnaFile}" | sed -e 's#/all_assembly_versions/#/latest_assembly_versions/#;'`
primaryAsm="${outside}/${D}/${B}_assembly_structure/Primary_Assembly"
chr2acc="${primaryAsm}/assembled_chromosomes/chr2acc"
unplacedScafAgp="${primaryAsm}/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz"
chr2scaf="${primaryAsm}/unlocalized_scaffolds/unlocalized.chr2scaf"
nonNucChr2acc="${primaryAsm}/non-nuclear/assembled_chromosomes/chr2acc"
nonNucChr2scaf="${primaryAsm}/non-nuclear/unlocalized_scaffolds/unlocalized.chr2acc"
# to count if any actual parts are being used, may not find any
export partCount=0
# assembly is unplaced contigs only if there are no other parts
export unplacedOnly=1

if [ -s "${inside}/${D}/${B}.checkAgpStatusOK.txt" ]; then
  echo "already done ${B}" 1>&2
  exit 0
fi
###########################################################################
# there will always be a 2bit file constructed from the fnaFile
mkdir -p "${inside}/${D}"
if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.ncbi.2bit" ]; then
  echo "NCBI 2bit ${B}" 1>&2
  faToTwoBit "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.2bit"
  twoBitInfo "${inside}/${D}/${B}.ncbi.2bit" stdout \
    | sort -k2nr > "${inside}/${D}/${B}.ncbi.chrom.sizes"
  touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.2bit"
  touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.chrom.sizes"
fi

###########################################################################
# first part could be assembled chromosomes
if [ -s "${chr2acc}" ]; then
  echo "chroms ${B}" 1>&2
  unplacedOnly=0
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.chr.fa.gz ]; then
    ${inside}/scripts/ucscCompositeAgp.pl "${primaryAsm}" \
      | gzip -c > "${inside}/${D}/${B}.chr.agp.gz"
    touch -r "${chr2acc}" "${inside}/${D}/${B}.chr.agp.gz"
    ${inside}/scripts/ucscComposeFasta.pl \
      "${primaryAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.chr.fa.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.chr.fa.gz
  fi
partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
fi

###########################################################################
# second part could be unlocalized scaffolds
if [ -s "${chr2scaf}" ]; then
  echo "unlocalized_scaffolds ${B}" 1>&2
  unplacedOnly=0
  if [ "${chr2scaf}" -nt "${inside}/${D}/${B}.unlocalized.agp.gz" ]; then
    ${inside}/scripts/unlocalizedAgp.pl "${primaryAsm}" \
        | gzip -c > "${inside}/${D}/${B}.unlocalized.agp.gz"
    touch -r "${chr2scaf}" "${inside}/${D}/${B}.unlocalized.agp.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unlocalized.fa.gz" ]; then
    ${inside}/scripts/unlocalFasta.pl "${primaryAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.unlocalized.fa.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.unlocalized.fa.gz"
  fi
partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
fi

###########################################################################
# third part could be unplaced scaffolds
# or, if this is the only part, then do not add "chrUn_" prefix to the
# contig identifiers.
if [ -s "${unplacedScafAgp}" ]; then
  echo "unplaced_scaffolds ${B}" 1>&2
#  if [ "${unplacedOnly}" -gt 0 ]; then
#    rm -f "${inside}/${D}/${B}.unplaced.agp.gz"
#    rm -f "${inside}/${D}/${B}.unplaced.fa.gz"
#  fi
  if [ "${unplacedScafAgp}" -nt "${inside}/${D}/${B}.unplaced.agp.gz" ]; then
    if [ "${unplacedOnly}" -gt 0 ]; then
      ${inside}/scripts/unplaceAgp.pl "" "${unplacedScafAgp}" \
         | gzip -c > "${inside}/${D}/${B}.unplaced.agp.gz"
    else
      ${inside}/scripts/unplaceAgp.pl "chrUn_" "${unplacedScafAgp}" \
         | gzip -c > "${inside}/${D}/${B}.unplaced.agp.gz"
    fi
    touch -r "${unplacedScafAgp}" "${inside}/${D}/${B}.unplaced.agp.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unplaced.fa.gz" ]; then
    if [ "${unplacedOnly}" -gt 0 ]; then
      ${inside}/scripts/unplaceFasta.pl "" "${unplacedScafAgp}" \
         "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
            > "${inside}/${D}/${B}.unplaced.fa.gz"
    else
      ${inside}/scripts/unplaceFasta.pl "chrUn_" "${unplacedScafAgp}" \
         "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
            > "${inside}/${D}/${B}.unplaced.fa.gz"
    fi
    touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.unplaced.fa.gz"
  fi
partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
fi

###########################################################################
# alternate sequences TO BE DONE

###########################################################################
# non-nuclear business TO BE DONE
if [ -s "${nonNucChr2acc}" ]; then
 echo "non-nuclear chroms ${B}" 1>&2
fi
if [ -s "${nonNucChr2scaf}" ]; then
 echo "non-nuclear unlocalized_scaffolds ${B}" 1>&2
fi

###########################################################################
# finally, construct the UCSC 2bit file if there are parts to go into
# this assembly
if [ ${partCount} -gt 0 ]; then
#  if [ "${unplacedOnly}" -gt 0 ]; then
#    rm -f "${inside}/${D}/${B}.ucsc.2bit"
#  fi
  echo "constructing UCSC 2bit file ${B}" 1>&2
  if [ ! -s "${inside}/${D}/${B}.ucsc.2bit" ]; then
    faToTwoBit ${inside}/${D}/${B}.*.fa.gz \
        "${inside}/${D}/${B}.ucsc.2bit"
    twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
        | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.ucsc.2bit"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.ucsc.chrom.sizes"
  fi
fi

###########################################################################
# error report
export wcNcbi=`cat "${inside}/${D}/${B}.ncbi.chrom.sizes" | wc -l`
export wcUcsc=0
if [ -s "${inside}/${D}/${B}.ucsc.chrom.sizes" ]; then
  wcUcsc=`cat "${inside}/${D}/${B}.ucsc.chrom.sizes" | wc -l`
fi
if [ "${wcNcbi}" -ne "${wcUcsc}" ]; then
  echo "ERROR: chrom.sizes count different ${wcNcbi} vs. ${wcUcsc}" 1>&2
  echo "${inside}/${D}" 1>&2
  exit 255
fi
if [ -s "${inside}/${D}/${B}.ucsc.2bit" ]; then
  rm -f "${inside}/${D}/${B}.checkAgpStatusOK.txt"
  checkAgp=`zcat ${inside}/${D}/*.agp.gz | grep -v "^#" | checkAgpAndFa stdin "${inside}/${D}/${B}.ucsc.2bit" 2>&1 | tail -1`
  if [ "${checkAgp}" != "All AGP and FASTA entries agree - both files are valid" ]; then
    echo "ERROR: checkAgpAndFa failed" 1>&2
    echo "${inside}/${D}" 1>&2
    exit 255
  else
    echo "${checkAgp}" > "${inside}/${D}/${B}.checkAgpStatusOK.txt"
    date "+%FT%T %s" >> "${inside}/${D}/${B}.checkAgpStatusOK.txt"
  fi
fi
exit 0
