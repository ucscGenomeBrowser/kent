#!/bin/bash

set -beEu -o pipefail

export outside="/hive/data/outside/ncbi/genomes/genbank"
export inside="/hive/data/inside/ncbi/genomes/genbank"
export dateStamp=`date "+%FT%T %s"`

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
printf "# %s ncbiToUcsc.sh %s\n" "${dateStamp}" "${fnaFile}" 1>&2

B=`basename "${fnaFile}" | sed -e 's/_genomic.fna.gz//;'`
D=`dirname "${fnaFile}" | sed -e 's#/all_assembly_versions/#/latest_assembly_versions/#;'`
asmReport=`echo "${fnaFile}" | sed -e 's/_genomic.fna.gz/_assembly_report.txt/;'`
primaryAsm="${outside}/${D}/${B}_assembly_structure/Primary_Assembly"
asmStructure="${outside}/${D}/${B}_assembly_structure"
rmOut="${outside}/${D}/${B}_rm.out.gz"
chr2acc="${primaryAsm}/assembled_chromosomes/chr2acc"
unplacedScafAgp="${primaryAsm}/unplaced_scaffolds/AGP/unplaced.scaf.agp.gz"
chr2scaf="${primaryAsm}/unlocalized_scaffolds/unlocalized.chr2scaf"
nonNucAsm="${outside}/${D}/${B}_assembly_structure/non-nuclear"
nonNucChr2acc="${nonNucAsm}/assembled_chromosomes/chr2acc"
nonNucChr2scaf="${nonNucAsm}/unlocalized_scaffolds/unlocalized.chr2scaf"
# to count if any actual parts are being used, may not find any
export partCount=0
# assembly is unplaced contigs only if there are no other parts
export unplacedOnly=1

# if checkAgpStatusOK.txt does not exist, run through that construction
#   procedure, else continue with bbi file construction
if [ ! -s "${inside}/${D}/${B}.checkAgpStatusOK.txt" ]; then

###########################################################################
# there will always be a 2bit file constructed from the fnaFile
mkdir -p "${inside}/${D}"
if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.ncbi.2bit" ]; then
  echo "# ${dateStamp} NCBI 2bit ${B}" 1>&2
  faToTwoBit "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.2bit"
  twoBitInfo "${inside}/${D}/${B}.ncbi.2bit" stdout \
    | sort -k2nr > "${inside}/${D}/${B}.ncbi.chrom.sizes"
  touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.2bit"
  touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.chrom.sizes"
fi

###########################################################################
# first part could be assembled chromosomes
if [ -s "${chr2acc}" ]; then
  echo "# ${dateStamp} chroms ${B}" 1>&2
  unplacedOnly=0
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.chr.fa.gz ]; then
    ${inside}/scripts/compositeAgp.pl ucsc "${primaryAsm}" \
      | gzip -c > "${inside}/${D}/${B}.chr.agp.gz"
    touch -r "${chr2acc}" "${inside}/${D}/${B}.chr.agp.gz"
    ${inside}/scripts/compositeAgp.pl ncbi "${primaryAsm}" \
      | gzip -c > "${inside}/${D}/${B}.chr.agp.ncbi.gz"
    touch -r "${chr2acc}" "${inside}/${D}/${B}.chr.agp.ncbi.gz"
    ${inside}/scripts/compositeFasta.pl ucsc \
      "${primaryAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.chr.fa.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.chr.fa.gz
    ${inside}/scripts/compositeFasta.pl ncbi \
      "${primaryAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.chr.fa.ncbi.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.chr.fa.ncbi.gz
  fi
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
else
  echo "# ${dateStamp} not found: ${chr2acc}" 1>&2
fi

###########################################################################
# second part could be unlocalized scaffolds
if [ -s "${chr2scaf}" ]; then
  echo "# ${dateStamp} unlocalized_scaffolds ${B}" 1>&2
  unplacedOnly=0
  if [ "${chr2scaf}" -nt "${inside}/${D}/${B}.unlocalized.agp.gz" ]; then
    ${inside}/scripts/unlocalizedAgp.pl ucsc "${primaryAsm}" \
        | gzip -c > "${inside}/${D}/${B}.unlocalized.agp.gz"
    touch -r "${chr2scaf}" "${inside}/${D}/${B}.unlocalized.agp.gz"
  fi
  if [ "${chr2scaf}" -nt "${inside}/${D}/${B}.unlocalized.agp.ncbi.gz" ]; then
    ${inside}/scripts/unlocalizedAgp.pl ncbi "${primaryAsm}" \
        | gzip -c > "${inside}/${D}/${B}.unlocalized.agp.ncbi.gz"
    touch -r "${chr2scaf}" "${inside}/${D}/${B}.unlocalized.agp.ncbi.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unlocalized.fa.gz" ]; then
    ${inside}/scripts/unlocalizedFasta.pl ucsc "${primaryAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.unlocalized.fa.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.unlocalized.fa.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unlocalized.fa.ncbi.gz" ]; then
    ${inside}/scripts/unlocalizedFasta.pl ncbi "${primaryAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.unlocalized.fa.ncbi.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.unlocalized.fa.ncbi.gz"
  fi
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
else
  echo "# ${dateStamp} not found: ${chr2scaf}" 1>&2
fi

###########################################################################
# third part could be alternate haplotypes
# haven't made the UCSC equivalent here yet

altCount=`(find "${asmStructure}" -type f | grep alt.scaf.agp.gz || true) | wc -l`

if [ "${altCount}" -gt 0 -a ! -s "${B}_alternates.agp.ncbi.gz" ]; then
  echo "# ${dateStamp} alternates ${B}" 1>&2
  ${inside}/scripts/alternatesAgp.pl ncbi "" "${asmStructure}" \
     | gzip -c > "${inside}/${D}/${B}.alternates.agp.ncbi.gz"
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
  touch -r "${inside}/${D}/${B}.ncbi.2bit" "${inside}/${D}/${B}.alternates.agp.ncbi.gz"
fi

###########################################################################
# fourth part could be unplaced scaffolds
# or, if this is the only part, then do not add "chrUn_" prefix to the
# contig identifiers.
if [ -s "${unplacedScafAgp}" ]; then
  echo "# ${dateStamp} unplaced_scaffolds ${B}" 1>&2
  if [ "${unplacedScafAgp}" -nt "${inside}/${D}/${B}.unplaced.agp.gz" ]; then
    if [ "${unplacedOnly}" -gt 0 ]; then
      ${inside}/scripts/unplacedAgp.pl ucsc "" "${unplacedScafAgp}" \
         | gzip -c > "${inside}/${D}/${B}.unplaced.agp.gz"
    else
      ${inside}/scripts/unplacedAgp.pl ucsc "chrUn_" "${unplacedScafAgp}" \
         | gzip -c > "${inside}/${D}/${B}.unplaced.agp.gz"
    fi
    touch -r "${unplacedScafAgp}" "${inside}/${D}/${B}.unplaced.agp.gz"
    ${inside}/scripts/unplacedAgp.pl ncbi "" "${unplacedScafAgp}" \
         | gzip -c > "${inside}/${D}/${B}.unplaced.agp.ncbi.gz"
    touch -r "${unplacedScafAgp}" "${inside}/${D}/${B}.unplaced.agp.ncbi.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unplaced.fa.gz" ]; then
    if [ "${unplacedOnly}" -gt 0 ]; then
      ${inside}/scripts/unplacedFasta.pl ucsc "" "${unplacedScafAgp}" \
         "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
            > "${inside}/${D}/${B}.unplaced.fa.gz"
    else
      ${inside}/scripts/unplacedFasta.pl ucsc "chrUn_" "${unplacedScafAgp}" \
         "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
            > "${inside}/${D}/${B}.unplaced.fa.gz"
    fi
    touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.unplaced.fa.gz"
    ${inside}/scripts/unplacedFasta.pl ncbi "" "${unplacedScafAgp}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.unplaced.fa.ncbi.gz"
    touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.unplaced.fa.ncbi.gz"
  fi
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
else
  echo "# ${dateStamp} not found: ${unplacedScafAgp}" 1>&2
fi

###########################################################################
# non-nuclear business
if [ -s "${nonNucChr2acc}" ]; then
  printf "# %s non-nuclear chroms %s\n" "${dateStamp}" "${B}" 1>&2
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.nonNucChr.fa.gz ]; then
    printf "# %s compositeAgp.pl ucsc %s\n" "${dateStamp}" "${nonNucAsm}" 1>&2
    ${inside}/scripts/compositeAgp.pl ucsc "${nonNucAsm}" \
      | gzip -c > "${inside}/${D}/${B}.nonNucChr.agp.gz"
    touch -r "${nonNucChr2acc}" "${inside}/${D}/${B}.nonNucChr.agp.gz"
    ${inside}/scripts/compositeFasta.pl ucsc \
      "${nonNucAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.nonNucChr.fa.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.nonNucChr.fa.gz
  fi
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.nonNucChr.fa.ncbi.gz ]; then
    printf "# %s compositeAgp.pl ncbi %s\n" "${dateStamp}" "${nonNucAsm}" 1>&2
    ${inside}/scripts/compositeAgp.pl ncbi "${nonNucAsm}" \
      | gzip -c > "${inside}/${D}/${B}.nonNucChr.agp.ncbi.gz"
    touch -r "${nonNucChr2acc}" "${inside}/${D}/${B}.nonNucChr.agp.ncbi.gz"
    echo "${inside}/scripts/compositeFasta.pl" "${nonNucAsm}" "${inside}/${D}/${B}.ncbi.2bit" 1>&2
    ${inside}/scripts/compositeFasta.pl ncbi \
      "${nonNucAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.nonNucChr.fa.ncbi.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.nonNucChr.fa.ncbi.gz
  fi
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
fi
if [ -s "${nonNucChr2scaf}" ]; then
  echo "# ${dateStamp} non-nuclear unlocalized_scaffolds ${B}" 1>&2
  if [ "${nonNucChr2scaf}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz" ]; then
    ${inside}/scripts/unlocalizedAgp.pl ucsc "${nonNucAsm}" \
        | gzip -c > "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz"
    touch -r "${nonNucChr2scaf}" "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz"
  fi
  if [ "${nonNucChr2scaf}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.agp.ncbi.gz" ]; then
    printf "# %s unlocalizedAgp.pl ncbi %s\n" "${dateStamp}" "${nonNucAsm}" 1>&2
    ${inside}/scripts/unlocalizedAgp.pl ncbi "${nonNucAsm}" \
        | gzip -c > "${inside}/${D}/${B}.nonNucUnlocalized.agp.ncbi.gz"
    touch -r "${nonNucChr2scaf}" "${inside}/${D}/${B}.nonNucUnlocalized.agp.ncbi.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz" ]; then
    ${inside}/scripts/unlocalizedFasta.pl ucsc "${nonNucAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.fa.ncbi.gz" ]; then
    ${inside}/scripts/unlocalizedFasta.pl ncbi "${nonNucAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.nonNucUnlocalized.fa.ncbi.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.nonNucUnlocalized.fa.ncbi.gz"
  fi
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
fi

###########################################################################
# finally, construct the UCSC 2bit file if there are parts to go into
# this assembly.  For no parts, make a noStructure assembly.
echo "# ${dateStamp} partCount: ${partCount}" 1>&2

if [ 0 -eq 1 ]; then
if [ ${partCount} -eq 0 ]; then
  echo "# ${dateStamp} constructing no structure assembly" 1>&2
  echo "# ${dateStamp} rm -f \"${inside}/${D}/${B}_noStructure.agp.ncbi.gz\"" 1>&2
  rm -f "${inside}/${D}/${B}_noStructure.agp.ncbi.gz"
  echo "# ${dateStamp} rm -f \"${inside}/${D}/${B}_noStructure..agp.ncbi.gz\"" 1>&2
  rm -f "${inside}/${D}/${B}_noStructure..agp.ncbi.gz"
  # check the sizes of the contig names in the ucsc.chrom.size file
  # to verify they are less than 31 characters
  if [ "${outside}/${asmReport}" -nt "${inside}/${D}/${B}_noStructure.agp.gz" ]; then
    contigCount=`(grep -v "^#" ${outside}/${asmReport} | wc -l) || true`
    if [ "${contigCount}" -gt 0 ]; then
    echo "${inside}/scripts/noStructureAgp.pl ucsc \"${outside}/${asmReport}\" \"${inside}/${D}/${B}.ncbi.chrom.sizes\" | gzip -c > \"${inside}/${D}/${B}_noStructure.agp.gz\"" 1>&2
    ${inside}/scripts/noStructureAgp.pl ucsc "${outside}/${asmReport}" \
      "${inside}/${D}/${B}.ncbi.chrom.sizes" | gzip -c \
        > "${inside}/${D}/${B}_noStructure.agp.gz"
    touch -r "${outside}/${asmReport}" "${inside}/${D}/${B}_noStructure.agp.gz"
    echo "${inside}/scripts/noStructureAgp.pl ncbi \"${outside}/${asmReport}\" \"${inside}/${D}/${B}.ncbi.chrom.sizes\" | gzip -c > \"${inside}/${D}/${B}_noStructure..agp.ncbi.gz\"" 1>&2
    ${inside}/scripts/noStructureAgp.pl ncbi "${outside}/${asmReport}" \
      "${inside}/${D}/${B}.ncbi.chrom.sizes" | gzip -c \
        > "${inside}/${D}/${B}_noStructure.agp.ncbi.gz"
    touch -r "${outside}/${asmReport}" "${inside}/${D}/${B}_noStructure.agp.ncbi.gz"
    echo "${inside}/scripts/noStructureFasta.pl ucsc \"${inside}/${D}/${B}_noStructure.agp.gz\" \"${outside}/${fnaFile}\" | faToTwoBit stdin \"${inside}/${D}/${B}.ucsc.2bit\"" 1>&2
    ${inside}/scripts/noStructureFasta.pl ucsc \
      "${inside}/${D}/${B}_noStructure.agp.gz" "${outside}/${fnaFile}" \
         | faToTwoBit stdin "${inside}/${D}/${B}.ucsc.2bit"
    twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
        | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.ucsc.2bit"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.ucsc.chrom.sizes"
    # verify contig name lengths are less than 31
    maxNameLength=`cut -f1 "${inside}/${D}/${B}.ucsc.chrom.sizes" | awk '{print length($1)}' | sort -nr | head -1`
    else
       maxNameLength=32
    fi
    # if not, then rebuild ucsc.2bit from ncbi.2bit
    if [ "${maxNameLength}" -gt 31 ]; then
       echo "# ${dateStamp} rebuilding UCSC no structure assembly from ncbi $maxNameLength" 1>&2
       rm -f "${inside}/${D}/${B}.ucsc.2bit"
       rm -f "${inside}/${D}/${B}.ucsc.chrom.sizes"
       twoBitToFa "${inside}/${D}/${B}.ncbi.2bit" stdout \
         | sed -e 's#\.\([0-9][0-9]*$\)#v\1#;' \
            | faToTwoBit stdin "${inside}/${D}/${B}.ucsc.2bit"
       touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.2bit"
       twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
           | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
       rm -f ${inside}/${D}/*agp.gz
       twoBitToFa "${inside}/${D}/${B}.ucsc.2bit" stdout \
         | hgFakeAgp stdin stdout | gzip -c > "${inside}/${D}/${B}.fake.agp.gz"
     touch -r "${inside}/${D}/${B}.ucsc.2bit" "${inside}/${D}/${B}.fake.agp.gz"
       rm -f ${inside}/${D}/*agp.ncbi.gz
       twoBitToFa "${inside}/${D}/${B}.ncbi.2bit" stdout \
    | hgFakeAgp stdin stdout | gzip -c > "${inside}/${D}/${B}.fake.agp.ncbi.gz"
 touch -r "${inside}/${D}/${B}.ncbi.2bit" "${inside}/${D}/${B}.fake.agp.ncbi.gz"
    fi
  fi
else
  if [ -s "${nonNucChr2scaf}" -o -s "${nonNucChr2acc}" ]; then
    rm -f "${inside}/${D}/${B}.ucsc.2bit"
  fi
  echo "# ${dateStamp} constructing UCSC 2bit file ${B}" 1>&2
  if [ ! -s "${inside}/${D}/${B}.ucsc.2bit" ]; then
    faToTwoBit ${inside}/${D}/${B}.*.fa.gz \
        "${inside}/${D}/${B}.ucsc.2bit"
    twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
        | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
    touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.2bit"
    touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.chrom.sizes"
    # verify contig name lengths are less than 31
    maxNameLength=`cut -f1 "${inside}/${D}/${B}.ucsc.chrom.sizes" | awk '{print length($1)}' | sort -nr | head -1`
    # if not, then rebuild ucsc.2bit from ncbi.2bit
    if [ "${maxNameLength}" -gt 31 ]; then
       rm -f "${inside}/${D}/${B}.ucsc.2bit"
       rm -f "${inside}/${D}/${B}.ucsc.chrom.sizes"
       twoBitToFa "${inside}/${D}/${B}.ncbi.2bit" stdout \
         | sed -e 's#\.\([0-9][0-9]*$\)#v\1#;' \
            | faToTwoBit stdin "${inside}/${D}/${B}.ucsc.2bit"
       touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.2bit"
       twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
           | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
       rm -f ${inside}/${D}/*agp.gz
       twoBitToFa "${inside}/${D}/${B}.ucsc.2bit" stdout \
         | hgFakeAgp stdin stdout | gzip -c > "${inside}/${D}/${B}.fake.agp.gz"
     touch -r "${inside}/${D}/${B}.ucsc.2bit" "${inside}/${D}/${B}.fake.agp.gz"
    fi
  fi
fi

# XXX temporary avoid UCSC 2bit construction
fi

###########################################################################
# error report

echo "# ${dateStamp} verify NCBI 2bit file ${B}" 1>&2
zcat ${inside}/${D}/*.agp.ncbi.gz | grep -v "^#" | checkAgpAndFa stdin "${inside}/${D}/${B}.ncbi.2bit" 2>&1 | tail -1
echo "# ${dateStamp} verify NCBI 2bit file ${B}" 1>&2

checkAgp=`zcat ${inside}/${D}/*.agp.ncbi.gz | grep -v "^#" | checkAgpAndFa stdin "${inside}/${D}/${B}.ncbi.2bit" 2>&1 | tail -1`
if [ "${checkAgp}" != "All AGP and FASTA entries agree - both files are valid" ]; then
    echo "# ${dateStamp} ERROR: NCBI checkAgpAndFa failed" 1>&2
    echo "# ${inside}/${D}" 1>&2
    exit 255
else
    echo "${checkAgp}" > "${inside}/${D}/${B}.checkAgpStatusOK.txt"
    date "+%FT%T %s" >> "${inside}/${D}/${B}.checkAgpStatusOK.txt"
fi

export wcNcbi=`cat "${inside}/${D}/${B}.ncbi.chrom.sizes" | wc -l`
export wcUcsc=0

if [ -s "${inside}/${D}/${B}.ucsc.chrom.sizes" ]; then
  wcUcsc=`cat "${inside}/${D}/${B}.ucsc.chrom.sizes" | wc -l`
fi

if [ "${wcNcbi}" -ne "${wcUcsc}" ]; then
  echo "# ${dateStamp} ERROR: NCBI vs. UCSC chrom.sizes count different ${wcNcbi} vs. ${wcUcsc}" 1>&2
  echo "# ${inside}/${D}" 1>&2
fi

if [ -s "${inside}/${D}/${B}.ucsc.2bit" ]; then
  checkAgp=`zcat ${inside}/${D}/*.agp.gz | grep -v "^#" | checkAgpAndFa stdin "${inside}/${D}/${B}.ucsc.2bit" 2>&1 | tail -1`
  if [ "${checkAgp}" != "All AGP and FASTA entries agree - both files are valid" ]; then
    echo "# ${dateStamp} ERROR: UCSC checkAgpAndFa failed" 1>&2
    echo "# ${inside}/${D}" 1>&2
  fi
fi

# above is performed when checkAgpStatusOK.txt does not exist
fi

###########################################################################
# bbi track file construction

if [ -s "${inside}/${D}/${B}.checkAgpStatusOK.txt" ]; then
  echo "# ${dateStamp} constructing bbi files" 1>&2

  mkdir -p "${inside}/${D}/bbi"
# XXX disable ucsc build
if [ 0 -eq 1 ]; then
  if [ "${inside}/${D}/${B}.checkAgpStatusOK.txt" -nt "${inside}/${D}/bbi/${B}.assembly.bb" ]; then
    echo "# ${dateStamp} constructing ucsc assembly bbi" 1>&2
    ${inside}/scripts/agpToBbi.sh ucsc "${B}" \
      "${inside}/${D}/${B}.ucsc.chrom.sizes" \
         "${inside}/${D}" "${inside}/${D}/bbi" 
  fi
fi
  if [ "${inside}/${D}/${B}.checkAgpStatusOK.txt" -nt "${inside}/${D}/bbi/${B}.assembly.ncbi.bb" ]; then
    echo "# ${dateStamp} constructing ncbi assembly bbi" 1>&2
    ${inside}/scripts/agpToBbi.sh ncbi "${B}" \
      "${inside}/${D}/${B}.ncbi.chrom.sizes" \
         "${inside}/${D}" "${inside}/${D}/bbi" 
  fi

  # XXX temporary force rebuild description.html
  rm -f "${inside}/${D}/${B}.description.html"
  if [ "${outside}/${asmReport}" -nt "${inside}/${D}/${B}.description.html" ]; then
    echo "# ${dateStamp} constructing description.html" 1>&2
    ${inside}/scripts/gatewayPage.pl ${outside}/${asmReport} \
      > "${inside}/${D}/${B}.description.html" \
      2> "${inside}/${D}/${B}.names.tab"
    ${inside}/scripts/buildStats.pl "${inside}/${D}/${B}.ncbi.chrom.sizes" \
      >> "${inside}/${D}/${B}.description.html" \
        2> "${inside}/${D}/${B}.build.stats.txt"
# XXX disable ucsc build
if [ 0 -eq 1 ]; then
    ${inside}/scripts/buildStats.pl "${inside}/${D}/${B}.ucsc.chrom.sizes" \
      >> "${inside}/${D}/${B}.description.html" \
        2> "${inside}/${D}/${B}.build.stats.txt"
fi
  fi

# XXX disable ucsc build
if [ 0 -eq 1 ]; then
  if [ "${inside}/${D}/${B}.ucsc.2bit" -nt "${inside}/${D}/bbi/${B}.gc5Base.bw" ]; then
    echo "# ${dateStamp} constructing ucsc gc5Base.bw" 1>&2
    hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \
      "${inside}/${D}/${B}.ucsc.2bit" \
        | gzip -c > "${inside}/${D}/${B}.gc5Base.wigVarStep.gz"
    wigToBigWig "${inside}/${D}/${B}.gc5Base.wigVarStep.gz" \
       "${inside}/${D}/${B}.ucsc.chrom.sizes" \
         "${inside}/${D}/bbi/${B}.gc5Base.bw"
    rm -f ${inside}/${D}/${B}.gc5Base.wigVarStep.gz
  fi
fi
  if [ "${inside}/${D}/${B}.ncbi.2bit" -nt "${inside}/${D}/bbi/${B}.gc5Base.ncbi.bw" ]; then
    echo "# ${dateStamp} constructing ncbi gc5Base.bw" 1>&2
    hgGcPercent -wigOut -doGaps -file=stdout -win=5 -verbose=0 test \
      "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > "${inside}/${D}/${B}.gc5Base.wigVarStep.ncbi.gz"
    wigToBigWig "${inside}/${D}/${B}.gc5Base.wigVarStep.ncbi.gz" \
       "${inside}/${D}/${B}.ncbi.chrom.sizes" \
         "${inside}/${D}/bbi/${B}.gc5Base.ncbi.bw"
    rm -f ${inside}/${D}/${B}.gc5Base.wigVarStep.ncbi.gz
  fi
  if [ "${rmOut}" -nt "${inside}/${D}/${B}.rmsk.class.profile.txt" ]; then
     echo "# ${dateStamp} ${inside}/scripts/rmsk.sh \"${rmOut}\" \"${inside}/${D}/\"" 1>&2
     ${inside}/scripts/rmsk.sh "${rmOut}" "${inside}/${D}/" \
        >> "${inside}/${D}/rmsk.process.log" 2>&1 || true
     if [ -s "${inside}/${D}/${B}.rmsk.class.profile.txt" ]; then
       touch -r "${rmOut}" "${inside}/${D}/${B}.rmsk.class.profile.txt"
     fi
  fi

  # cpg.sh will do its own checking to see if it needs to run
  printf "# %s cpg.sh %s\n" "${dateStamp}" "${B}" 1>&2
  ${inside}/scripts/cpg.sh "${inside}/${D}"

  # XXX always rebuild trackDb.txt
  rm -f "${inside}/${D}/trackDb.txt"
  ${inside}/scripts/trackDb.sh ucsc "${B}" "${inside}/${D}" \
      > "${inside}/${D}/${B}.trackDb.txt"
  # might be zero length, if so remove it
  if [ ! -s "${inside}/${D}/${B}.trackDb.txt" ]; then
     rm -f "${inside}/${D}/${B}.trackDb.txt"
  fi
  ${inside}/scripts/trackDb.sh ncbi "${B}" "${inside}/${D}" \
      > "${inside}/${D}/${B}.trackDb.ncbi.txt"
  # might be zero length, if so remove it
  if [ ! -s "${inside}/${D}/${B}.trackDb.ncbi.txt" ]; then
     rm -f "${inside}/${D}/${B}.trackDb.ncbi.txt"
  fi

  printf "# %s %s.assembly.html\n" "${dateStamp}" "${B}" 1>&2
  ${inside}/scripts/assemblyDescription.pl "${inside}/${D}"
  printf "# %s %s.gap.html\n" "${dateStamp}" "${B}" 1>&2
  ${inside}/scripts/gapDescription.pl "${inside}/${D}"

fi

exit 0
