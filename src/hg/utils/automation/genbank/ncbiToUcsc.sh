#!/bin/bash

set -beEu -o pipefail

function usage() {
  printf "usage: ucscToNcbi.sh <genbank|refseq> <pathTo>/*_genomic.fna.gz
  select genbank or refseq assembly hierarchy,
  the unrooted <pathTo> is to a file in .../all_assembly_versions/...
  relative to the directory hierarchy:
    %s
expecting to find corresponding files in the same directory path
  with .../latest_assembly_versions/... instead of /all_.../
  results will be constructing files in corresponding directory
  relative to the directory hierarchy:
    %s\n" "${outside}" "${inside}" 1>&2
}

export outside="/hive/data/outside/ncbi/genomes/[refseq|genbank]"
export inside="/hive/data/inside/ncbi/genomes/[refseq|genbank]"
export dateStamp=`date "+%FT%T %s"`

if [ $# -lt 2 ]; then
  usage
  exit 255
fi

export asmType=$1

if [ "${asmType}" != "genbank" -a "${asmType}" != "refseq" ]; then
  printf "ERROR: select genbank or refseq assemblies update\n" 1>&2
  usage
  exit 255
fi

outside="/hive/data/outside/ncbi/genomes/$asmType"
inside="/hive/data/inside/ncbi/genomes/$asmType"
# expecting to find all the scripts used by this in the
# users 'kent' source tree copy
export scripts="$HOME/kent/src/hg/utils/automation/genbank"

export fnaFile=$2

printf "# %s ncbiToUcsc.sh %s\n" "${dateStamp}" "${fnaFile}" 1>&2

B=`basename "${fnaFile}" | sed -e 's/_genomic.fna.gz//;'`
D=`dirname "${fnaFile}" | sed -e 's#/all_assembly_versions/#/latest_assembly_versions/#;'`
asmReport=`echo "${fnaFile}" | sed -e 's/_genomic.fna.gz/_assembly_report.txt/;'`
primaryAsm="${outside}/${D}/${B}_assembly_structure/Primary_Assembly"
asmStructure="${outside}/${D}/${B}_assembly_structure"
rmOut="${outside}/${D}/${B}_rm.out.gz"
gffFile="${outside}/${D}/${B}_genomic.gff.gz"
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

if [ ! -s "${outside}/${asmReport}" ]; then
  printf "# ERROR: missing assembly report %s\n" "${asmReport}" 1>&2
  exit 255
fi

# if checkAgpStatusOK.txt does not exist, run through that construction
#   procedure, else continue with bbi file construction
### XXX temporary run everything
### rm -f "${inside}/${D}/${B}.checkAgpStatusOK.txt"
if [ ! -s "${inside}/${D}/${B}.checkAgpStatusOK.txt" ]; then

###########################################################################
# there will always be a 2bit file constructed from the fnaFile
mkdir -p "${inside}/${D}"
if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.ncbi.2bit" ]; then
  printf "# %s NCBI 2bit %s\n" "${dateStamp}" "${B}" 1>&2
  printf "# %s outside fna %s\n" "${dateStamp}" "${outside}/${fnaFile}" 1>&2
  faToTwoBit "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.2bit"
  twoBitInfo "${inside}/${D}/${B}.ncbi.2bit" stdout \
    | sort -k2nr > "${inside}/${D}/${B}.ncbi.chrom.sizes"
  touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.2bit"
  touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ncbi.chrom.sizes"
fi

###########################################################################
# first part could be assembled chromosomes
if [ -s "${chr2acc}" ]; then
  printf "# %s chroms %s\n" "${dateStamp}" "${B}" 1>&2
  unplacedOnly=0
### XXX temporary rebuild agp.gz
###  rm -f ${inside}/${D}/${B}.chr.agp.gz ${inside}/${D}/${B}.chr.fa.gz
  if [ "${chr2acc}" -nt ${inside}/${D}/${B}.chr.agp.gz ]; then
    ${scripts}/compositeAgp.pl ucsc "${primaryAsm}" \
      2> >(sort -u > "${inside}/${D}/${B}.ucsc.to.ncbi.chr.names") \
         | gzip -c > "${inside}/${D}/${B}.chr.agp.gz"
    touch -r "${chr2acc}" "${inside}/${D}/${B}.chr.agp.gz" \
      "${inside}/${D}/${B}.ucsc.to.ncbi.chr.names"
  fi
  if [ "${chr2acc}" -nt ${inside}/${D}/${B}.chr.agp..ncbi.gz ]; then
    ${scripts}/compositeAgp.pl ncbi "${primaryAsm}" \
      | gzip -c > "${inside}/${D}/${B}.chr.agp.ncbi.gz"
    touch -r "${chr2acc}" "${inside}/${D}/${B}.chr.agp.ncbi.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.chr.fa.gz ]; then
    ${scripts}/compositeFasta.pl ucsc \
      "${primaryAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.chr.fa.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.chr.fa.gz
  fi
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.chr.fa.ncbi.gz ]; then
    ${scripts}/compositeFasta.pl ncbi \
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
### XXX temporary rebuild agp
###  rm -f "${inside}/${D}/${B}.unlocalized.agp.gz" "${inside}/${D}/${B}.unlocalized.fa.gz"
  if [ "${chr2scaf}" -nt "${inside}/${D}/${B}.unlocalized.agp.gz" ]; then
    ${scripts}/unlocalizedAgp.pl ucsc "${primaryAsm}" \
      2> >(sort -u > "${inside}/${D}/${B}.ucsc.to.ncbi.unlocalized.names") \
        | gzip -c > "${inside}/${D}/${B}.unlocalized.agp.gz"
    touch -r "${chr2scaf}" "${inside}/${D}/${B}.unlocalized.agp.gz"
  fi
  if [ "${chr2scaf}" -nt "${inside}/${D}/${B}.unlocalized.agp.ncbi.gz" ]; then
    ${scripts}/unlocalizedAgp.pl ncbi "${primaryAsm}" \
        | gzip -c > "${inside}/${D}/${B}.unlocalized.agp.ncbi.gz"
    touch -r "${chr2scaf}" "${inside}/${D}/${B}.unlocalized.agp.ncbi.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unlocalized.fa.gz" ]; then
    ${scripts}/unlocalizedFasta.pl ucsc "${primaryAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.unlocalized.fa.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.unlocalized.fa.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unlocalized.fa.ncbi.gz" ]; then
    ${scripts}/unlocalizedFasta.pl ncbi "${primaryAsm}" \
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

if [ "${altCount}" -gt 0 ]; then
  if [ ! -s "${B}_alternates.agp.ncbi.gz" ]; then
    printf "# %s alternates %s" "${dateStamp}" "${B}" 1>&2
    ${scripts}/alternatesAgp.pl ncbi "" "${asmStructure}" \
       | gzip -c > "${inside}/${D}/${B}.alternates.agp.ncbi.gz"
    partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
    touch -r "${inside}/${D}/${B}.ncbi.2bit" "${inside}/${D}/${B}.alternates.agp.ncbi.gz"
  fi
fi

${scripts}/ucscPatchNames.sh "${outside}/${asmReport}" \
  > "${inside}/${D}/${B}.ucsc.to.ncbi.patch.names"
if [ -s "${inside}/${D}/${B}.ucsc.to.ncbi.patch.names" ]; then
 printf "# %s patches built %s\n" "${dateStamp}" "${B}"
else
 rm -f "${inside}/${D}/${B}.ucsc.to.ncbi.patch.names"
fi

${scripts}/ucscAltNames.sh "${outside}/${asmReport}" \
  > "${inside}/${D}/${B}.ucsc.to.ncbi.alt.names"
if [ -s "${inside}/${D}/${B}.ucsc.to.ncbi.alt.names" ]; then
 printf "# %s alternates built %s\n" "${dateStamp}" "${B}"
else
 rm -f "${inside}/${D}/${B}.ucsc.to.ncbi.alt.names"
fi

###########################################################################
# fourth part could be unplaced scaffolds
# or, if this is the only part, then do not add "chrUn_" prefix to the
# contig identifiers.
if [ -s "${unplacedScafAgp}" ]; then
  echo "# ${dateStamp} unplaced_scaffolds ${B}" 1>&2
### XXX temporary rebuild agp
###  rm -f "${inside}/${D}/${B}.unplaced.agp.gz"
  if [ "${unplacedScafAgp}" -nt "${inside}/${D}/${B}.unplaced.agp.gz" ]; then
    if [ "${unplacedOnly}" -gt 0 ]; then
      ${scripts}/unplacedAgp.pl ucsc "" "${unplacedScafAgp}" \
        2> >(sort -u > "${inside}/${D}/${B}.ucsc.to.ncbi.unplaced.names") \
           | gzip -c > "${inside}/${D}/${B}.unplaced.agp.gz"
    else
      ${scripts}/unplacedAgp.pl ucsc "chrUn_" "${unplacedScafAgp}" \
        2> >(sort -u > "${inside}/${D}/${B}.ucsc.to.ncbi.unplaced.names") \
           | gzip -c > "${inside}/${D}/${B}.unplaced.agp.gz"
    fi
    touch -r "${unplacedScafAgp}" "${inside}/${D}/${B}.unplaced.agp.gz"
    ${scripts}/unplacedAgp.pl ncbi "" "${unplacedScafAgp}" \
         | gzip -c > "${inside}/${D}/${B}.unplaced.agp.ncbi.gz"
    touch -r "${unplacedScafAgp}" "${inside}/${D}/${B}.unplaced.agp.ncbi.gz"
  fi
### XXX temporary rebuild fa
###  rm -f "${inside}/${D}/${B}.unplaced.fa.gz"
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.unplaced.fa.gz" ]; then
    if [ "${unplacedOnly}" -gt 0 ]; then
      ${scripts}/unplacedFasta.pl ucsc "" "${unplacedScafAgp}" \
         "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
            > "${inside}/${D}/${B}.unplaced.fa.gz"
    else
      ${scripts}/unplacedFasta.pl ucsc "chrUn_" "${unplacedScafAgp}" \
         "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
            > "${inside}/${D}/${B}.unplaced.fa.gz"
    fi
    touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.unplaced.fa.gz"
    ${scripts}/unplacedFasta.pl ncbi "" "${unplacedScafAgp}" \
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
### XXX temporary rebuild agp
###  rm -f ${inside}/${D}/${B}.nonNucChr.agp.gz
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.nonNucChr.agp.gz ]; then
    printf "# %s compositeAgp.pl ucsc %s\n" "${dateStamp}" "${nonNucAsm}" 1>&2
    ${scripts}/compositeAgp.pl ucsc "${nonNucAsm}" \
      2> >(sort -u > "${inside}/${D}/${B}.ucsc.to.ncbi.nonNucChr.names") \
         | gzip -c > "${inside}/${D}/${B}.nonNucChr.agp.gz"
    touch -r "${nonNucChr2acc}" "${inside}/${D}/${B}.nonNucChr.agp.gz"
  fi
### XXX temporary rebuild fa
###  rm -f ${inside}/${D}/${B}.nonNucChr.fa.gz
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.nonNucChr.fa.gz ]; then
    ${scripts}/compositeFasta.pl ucsc \
      "${nonNucAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.nonNucChr.fa.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.nonNucChr.fa.gz
  fi
  if [ "${outside}/${fnaFile}" -nt ${inside}/${D}/${B}.nonNucChr.fa.ncbi.gz ]; then
    printf "# %s compositeAgp.pl ncbi %s\n" "${dateStamp}" "${nonNucAsm}" 1>&2
    ${scripts}/compositeAgp.pl ncbi "${nonNucAsm}" \
      | gzip -c > "${inside}/${D}/${B}.nonNucChr.agp.ncbi.gz"
    touch -r "${nonNucChr2acc}" "${inside}/${D}/${B}.nonNucChr.agp.ncbi.gz"
    echo "${scripts}/compositeFasta.pl" "${nonNucAsm}" "${inside}/${D}/${B}.ncbi.2bit" 1>&2
    ${scripts}/compositeFasta.pl ncbi \
      "${nonNucAsm}" "${inside}/${D}/${B}.ncbi.2bit" \
        | gzip -c > ${inside}/${D}/${B}.nonNucChr.fa.ncbi.gz
    touch -r "${outside}/${fnaFile}" ${inside}/${D}/${B}.nonNucChr.fa.ncbi.gz
  fi
  partCount=`echo $partCount | awk '{printf "%d", $1+1}'`
fi
if [ -s "${nonNucChr2scaf}" ]; then
  echo "# ${dateStamp} non-nuclear unlocalized_scaffolds ${B}" 1>&2
### XXX temporary rebuild agp
###  rm -f "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz"
  if [ "${nonNucChr2scaf}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz" ]; then
    ${scripts}/unlocalizedAgp.pl ucsc "${nonNucAsm}" \
   2> >(sort -u > "${inside}/${D}/${B}.ucsc.to.ncbi.nonNucUnlocalized.names") \
        | gzip -c > "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz"
    touch -r "${nonNucChr2scaf}" "${inside}/${D}/${B}.nonNucUnlocalized.agp.gz"
  fi
  if [ "${nonNucChr2scaf}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.agp.ncbi.gz" ]; then
    printf "# %s unlocalizedAgp.pl ncbi %s\n" "${dateStamp}" "${nonNucAsm}" 1>&2
    ${scripts}/unlocalizedAgp.pl ncbi "${nonNucAsm}" \
        | gzip -c > "${inside}/${D}/${B}.nonNucUnlocalized.agp.ncbi.gz"
    touch -r "${nonNucChr2scaf}" "${inside}/${D}/${B}.nonNucUnlocalized.agp.ncbi.gz"
  fi
### XXX temporary rebuild fa
###  rm -f "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz"
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz" ]; then
    ${scripts}/unlocalizedFasta.pl ucsc "${nonNucAsm}" \
       "${inside}/${D}/${B}.ncbi.2bit" | gzip -c \
          > "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.nonNucUnlocalized.fa.gz"
  fi
  if [ "${outside}/${fnaFile}" -nt "${inside}/${D}/${B}.nonNucUnlocalized.fa.ncbi.gz" ]; then
    ${scripts}/unlocalizedFasta.pl ncbi "${nonNucAsm}" \
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
printf "# %s partCount: %s\n" "${dateStamp}" "${partCount}" 1>&2

namesCount=`(ls ${inside}/${D}/*.ucsc.to.ncbi.*.names || true) | wc -l`

if [ "${namesCount}" -gt 0 ]; then
  newestName=`ls -rt ${inside}/${D}/*.ucsc.to.ncbi.*.names | tail -1`
### XXX temporarily force rebuild agp.ucsc
###  rm -f "${inside}/${D}/${B}.agp.ucsc.gz"
  if [ "${newestName}" -nt "${inside}/${D}/${B}.agp.ucsc.gz" ]; then
    printf "# %s constructing %s UCSC agp file\n", "${dateStamp}" "${B}" 1>&2
    cat ${inside}/${D}/*.ucsc.to.ncbi.*.names \
       > "${inside}/${D}/ucscToNcbi.name.txt"
    zcat ${inside}/${D}/*.agp.ncbi.gz | ${scripts}/agpNameTranslate.pl \
      "${inside}/${D}/ucscToNcbi.name.txt" /dev/stdin \
        | gzip -c > "${inside}/${D}/${B}.agp.ucsc.gz"
    touch -r "${newestName}" "${inside}/${D}/${B}.agp.ucsc.gz" \
       "${inside}/${D}/${B}.ncbi.to.ucsc.sed"
    rm -f "${inside}/${D}/ucscToNcbi.name.txt"
    ${scripts}/ucscPatchAltsFa.sh "${inside}/${D}"
  fi
fi

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
### XXX temporary disable UCSC construction
if [ 0 -eq 1 ]; then
    echo "${scripts}/noStructureAgp.pl ucsc \"${outside}/${asmReport}\" \"${inside}/${D}/${B}.ncbi.chrom.sizes\" | gzip -c > \"${inside}/${D}/${B}_noStructure.agp.gz\"" 1>&2
    ${scripts}/noStructureAgp.pl ucsc "${outside}/${asmReport}" \
      "${inside}/${D}/${B}.ncbi.chrom.sizes" | gzip -c \
        > "${inside}/${D}/${B}_noStructure.agp.gz"
    touch -r "${outside}/${asmReport}" "${inside}/${D}/${B}_noStructure.agp.gz"
fi ### XXX temporary disable UCSC construction
    echo "${scripts}/noStructureAgp.pl ncbi \"${outside}/${asmReport}\" \"${inside}/${D}/${B}.ncbi.chrom.sizes\" | gzip -c > \"${inside}/${D}/${B}_noStructure..agp.ncbi.gz\"" 1>&2
    ${scripts}/noStructureAgp.pl ncbi "${outside}/${asmReport}" \
      "${inside}/${D}/${B}.ncbi.chrom.sizes" | gzip -c \
        > "${inside}/${D}/${B}_noStructure.agp.ncbi.gz"
    touch -r "${outside}/${asmReport}" "${inside}/${D}/${B}_noStructure.agp.ncbi.gz"
### XXX temporary disable UCSC construction
if [ 0 -eq 1 ]; then
    echo "${scripts}/noStructureFasta.pl ucsc \"${inside}/${D}/${B}_noStructure.agp.gz\" \"${outside}/${fnaFile}\" | faToTwoBit stdin \"${inside}/${D}/${B}.ucsc.2bit\"" 1>&2
    ${scripts}/noStructureFasta.pl ucsc \
      "${inside}/${D}/${B}_noStructure.agp.gz" "${outside}/${fnaFile}" \
         | faToTwoBit stdin "${inside}/${D}/${B}.ucsc.2bit"
    twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
        | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.ucsc.2bit"
    touch -r "${outside}/${fnaFile}" \
        "${inside}/${D}/${B}.ucsc.chrom.sizes"
    # verify contig name lengths are less than 31
    maxNameLength=`(cut -f1 "${inside}/${D}/${B}.ucsc.chrom.sizes" | awk '{print length($1)}' | sort -nr | head -1 || true)`
fi ### XXX temporary disable UCSC construction
       maxNameLength=32
    else
       maxNameLength=32
    fi
    # if not, then rebuild ucsc.2bit from ncbi.2bit
    if [ "${maxNameLength}" -gt 31 ]; then
       echo "# ${dateStamp} rebuilding UCSC no structure assembly from ncbi $maxNameLength" 1>&2
       rm -f "${inside}/${D}/${B}.ucsc.2bit"
       rm -f "${inside}/${D}/${B}.ucsc.chrom.sizes"
       rm -f ${inside}/${D}/*.ucsc.to.ncbi.*.names
       twoBitInfo "${inside}/${D}/${B}.ncbi.2bit" stdout \
         | cut -f1 | sed -e 's#\(.*\)\.\([0-9][0-9]*$\)#\1v\2\t\1.\2#;' \
            > ${inside}/${D}/${B}.ucsc.to.ncbi.fake.names
       twoBitToFa "${inside}/${D}/${B}.ncbi.2bit" stdout \
         | sed -e 's#\.\([0-9][0-9]*$\)#v\1#;' \
            | faToTwoBit stdin "${inside}/${D}/${B}.ucsc.2bit"
       touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.2bit"
       twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
           | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
       rm -f ${inside}/${D}/*agp.gz ${inside}/${D}/*agp.ucsc.gz
       twoBitToFa "${inside}/${D}/${B}.ucsc.2bit" stdout \
         | hgFakeAgp stdin stdout | gzip -c > "${inside}/${D}/${B}.fake.agp.gz"
     touch -r "${inside}/${D}/${B}.ucsc.2bit" "${inside}/${D}/${B}.fake.agp.gz"
       cp -p "${inside}/${D}/${B}.fake.agp.gz" "${inside}/${D}/${B}.agp.ucsc.gz"
       rm -f ${inside}/${D}/*agp.ncbi.gz
       twoBitToFa "${inside}/${D}/${B}.ncbi.2bit" stdout \
    | hgFakeAgp stdin stdout | gzip -c > "${inside}/${D}/${B}.fake.agp.ncbi.gz"
 touch -r "${inside}/${D}/${B}.ncbi.2bit" "${inside}/${D}/${B}.fake.agp.ncbi.gz"
    fi
  fi
else  ### there are part counts, build UCSC 2bit file from the parts
### XXX force rebuild of ucsc.2bit file
###  rm -f "${inside}/${D}/${B}.ucsc.2bit" "${inside}/${D}/${B}.ucsc.chrom.sizes"
  if [ -s "${inside}/${D}/${B}.agp.ucsc.gz" ]; then
    if [ -s "${nonNucChr2scaf}" -o -s "${nonNucChr2acc}" ]; then
      rm -f "${inside}/${D}/${B}.ucsc.2bit"
    fi
    printf "# %s constructing UCSC 2bit file %s\n" "${dateStamp}" "${B}" 1>&2
    if [ ! -s "${inside}/${D}/${B}.ucsc.2bit" ]; then
      printf "# %s faToTwoBit %s/%s/%s.*.fa.gz\n" "${dateStamp}" "${inside}" "${D}" "${B}" 1>&2
      faToTwoBit ${inside}/${D}/${B}.*.fa.gz \
          "${inside}/${D}/${B}.ucsc.2bit"
      twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
          | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
      touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.2bit"
      touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.chrom.sizes"
      # verify contig name lengths are less than 31
      maxNameLength=`(cut -f1 "${inside}/${D}/${B}.ucsc.chrom.sizes" | awk '{print length($1)}' | sort -nr | head -1 || true)`
      # if not, then rebuild ucsc.2bit from ncbi.2bit
      if [ "${maxNameLength}" -gt 31 ]; then
         printf "# %s rebuilding ucsc.2bit due to names > 31 characters %d\n" "${dateStamp}" "${maxNameLength}"
         rm -f "${inside}/${D}/${B}.ucsc.2bit"
         rm -f "${inside}/${D}/${B}.ucsc.chrom.sizes"
         rm -f ${inside}/${D}/*.ucsc.to.ncbi.*.names
         twoBitInfo "${inside}/${D}/${B}.ncbi.2bit" stdout \
           | cut -f1 | sed -e 's#\(.*\)\.\([0-9][0-9]*$\)#\1v\2\t\1.\2#;' \
              > ${inside}/${D}/${B}.ucsc.to.ncbi.fake.names
         twoBitToFa "${inside}/${D}/${B}.ncbi.2bit" stdout \
           | sed -e 's#\.\([0-9][0-9]*$\)#v\1#;' \
              | faToTwoBit stdin "${inside}/${D}/${B}.ucsc.2bit"
         touch -r "${outside}/${fnaFile}" "${inside}/${D}/${B}.ucsc.2bit"
         twoBitInfo "${inside}/${D}/${B}.ucsc.2bit" stdout \
             | sort -k2nr > "${inside}/${D}/${B}.ucsc.chrom.sizes"
         rm -f ${inside}/${D}/*agp.gz "${inside}/${D}/${B}.agp.ucsc.gz"
         twoBitToFa "${inside}/${D}/${B}.ucsc.2bit" stdout \
          | hgFakeAgp stdin stdout | gzip -c > "${inside}/${D}/${B}.fake.agp.gz"
      touch -r "${inside}/${D}/${B}.ucsc.2bit" "${inside}/${D}/${B}.fake.agp.gz"
       cp -p "${inside}/${D}/${B}.fake.agp.gz" "${inside}/${D}/${B}.agp.ucsc.gz"
      fi
    fi
  fi
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

printf "# %s verify UCSC 2bit file %s\n" "${dateStamp}" "${B}"  1>&2
if [ -s "${inside}/${D}/${B}.ucsc.2bit" ]; then
  checkAgp=`zcat ${inside}/${D}/${B}.agp.ucsc.gz | grep -v "^#" | checkAgpAndFa stdin "${inside}/${D}/${B}.ucsc.2bit" 2>&1 | tail -1`
  if [ "${checkAgp}" != "All AGP and FASTA entries agree - both files are valid" ]; then
    echo "# ${dateStamp} ERROR: UCSC checkAgpAndFa failed" 1>&2
    echo "# ${inside}/${D}" 1>&2
  fi
fi

# above is performed when checkAgpStatusOK.txt does not exist
fi

###########################################################################
# bbi track file construction

### XXX temprorary rebuild lift files
rm -f "${inside}/${D}/${B}.ucscToNcbi.lift"
if [ ! -s "${inside}/${D}/${B}.ucscToNcbi.lift" ]; then
  join -t'	' <(sort ${inside}/${D}/${B}.ucsc.chrom.sizes) \
     <(sort ${inside}/${D}/${B}.*.names) \
      | awk 'BEGIN{OFS="\t"}{printf "0\t%s\t%d\t%s\t%d\n",  $1,$2,$3,$2}' \
        > "${inside}/${D}/${B}.ucscToNcbi.lift"
  join -t'	' <(sort ${inside}/${D}/${B}.ucsc.chrom.sizes) \
     <(sort ${inside}/${D}/${B}.*.names) \
      | awk 'BEGIN{OFS="\t"}{printf "0\t%s\t%d\t%s\t%d\n",  $3,$2,$1,$2}' \
        > "${inside}/${D}/${B}.ncbiToUcsc.lift"
fi
if [ -s "${inside}/${D}/${B}.checkAgpStatusOK.txt" ]; then
  echo "# ${dateStamp} constructing bbi files" 1>&2

  mkdir -p "${inside}/${D}/bbi"
### XXX temporary rebuild assembly files and indexes
###  rm -f "${inside}/${D}/bbi/${B}.assembly.ncbi.bb"
  if [ "${inside}/${D}/${B}.checkAgpStatusOK.txt" -nt "${inside}/${D}/bbi/${B}.assembly.bb" ]; then
    echo "# ${dateStamp} constructing ucsc assembly bbi" 1>&2
    ${scripts}/agpToBbi.sh ucsc "${B}" \
      "${inside}/${D}/${B}.ucsc.chrom.sizes" \
         "${inside}/${D}" "${inside}/${D}/bbi" 
  fi
  if [ "${inside}/${D}/${B}.checkAgpStatusOK.txt" -nt "${inside}/${D}/bbi/${B}.assembly.ncbi.bb" ]; then
    echo "# ${dateStamp} constructing ncbi assembly bbi" 1>&2
    ${scripts}/agpToBbi.sh ncbi "${B}" \
      "${inside}/${D}/${B}.ncbi.chrom.sizes" \
         "${inside}/${D}" "${inside}/${D}/bbi" 
  fi

  # XXX always want to force rebuild of description.html
  rm -f "${inside}/${D}/${B}.description.html"
  if [ "${outside}/${asmReport}" -nt "${inside}/${D}/${B}.description.html" ]; then
    echo "# ${dateStamp} constructing description.html" 1>&2
    ${scripts}/gatewayPage.pl ${outside}/${asmReport} \
      > "${inside}/${D}/${B}.description.html" \
      2> "${inside}/${D}/${B}.names.tab"
    ${scripts}/buildStats.pl "${inside}/${D}/${B}.ncbi.chrom.sizes" \
      >> "${inside}/${D}/${B}.description.html" \
        2> "${inside}/${D}/${B}.build.stats.txt"
  fi

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
### XXX - rerun rmsk to get both done
###   rm -f "${inside}/${D}/${B}.rmsk.class.profile.txt"
  if [ "${rmOut}" -nt "${inside}/${D}/${B}.rmsk.class.profile.txt" ]; then
     echo "# ${dateStamp} ${scripts}/rmsk.sh \"${rmOut}\" \"${inside}/${D}/\"" 1>&2
     ${scripts}/rmsk.sh "${rmOut}" "${inside}/${D}/" \
        >> "${inside}/${D}/rmsk.process.log" 2>&1 || true
     if [ -s "${inside}/${D}/${B}.rmsk.class.profile.txt" ]; then
       touch -r "${rmOut}" "${inside}/${D}/${B}.rmsk.class.profile.txt"
     fi
  fi

  # ncbiGene.sh will do its own checking to see if it needs to run
  printf "# %s ncbiGene.sh %s\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/ncbiGene.sh "${gffFile}" "${inside}/${D}/"

  # cpg.sh is run separately from kluster jobs on hgwdev which spawns
  # cluster jobs on ku

  # cpg.sh will do its own checking to see if it needs to run
  # printf "# %s cpg.sh %s\n" "${dateStamp}" "${B}" 1>&2
  # ${scripts}/cpg.sh "${inside}/${D}"

  # construct a signature from faCount totals to compare with UCSC existing
  #  genome browsers

  if [ ! -s ${inside}/${D}/${B}.faCount.ucsc.txt ]; then
     if [ -s ${inside}/${D}/${B}.ucsc.2bit ]; then
       twoBitToFa ${inside}/${D}/${B}.ucsc.2bit stdout | faCount stdin \
          > ${inside}/${D}/${B}.faCount.ucsc.txt
     fi
  fi
  if [ ! -s ${inside}/${D}/${B}.faCount.ncbi.txt ]; then
     if [ -s ${inside}/${D}/${B}.ncbi.2bit ]; then
       twoBitToFa ${inside}/${D}/${B}.ncbi.2bit stdout | faCount stdin \
          > ${inside}/${D}/${B}.faCount.ncbi.txt
     fi
  fi
  if [ ! -s ${inside}/${D}/${B}.faCount.signature.txt ]; then
     twoBitToFa ${inside}/${D}/${B}.ncbi.2bit stdout | faCount stdin \
       |  grep -P "^total\t" > ${inside}/${D}/${B}.faCount.signature.txt
     touch -r ${inside}/${D}/${B}.ncbi.2bit \
       ${inside}/${D}/${B}.faCount.signature.txt
  fi

  # XXX always rebuild trackDb.txt
  rm -f "${inside}/${D}/trackDb.txt" "${inside}/${D}/${B}.trackDb.ncbi.txt"
  ${scripts}/trackDb.sh ucsc "${B}" "${inside}/${D}" \
      > "${inside}/${D}/${B}.trackDb.txt"
  # might be zero length, if so remove it
  if [ ! -s "${inside}/${D}/${B}.trackDb.txt" ]; then
     rm -f "${inside}/${D}/${B}.trackDb.txt"
  fi
  ${scripts}/trackDb.sh ncbi "${B}" "${inside}/${D}" \
      > "${inside}/${D}/${B}.trackDb.ncbi.txt"
  # might be zero length, if so remove it
  if [ ! -s "${inside}/${D}/${B}.trackDb.ncbi.txt" ]; then
     rm -f "${inside}/${D}/${B}.trackDb.ncbi.txt"
  fi

  printf "# %s %s.assembly.html\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/assemblyDescription.pl "${inside}/${D}"
  printf "# %s %s.gap.html\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/gapDescription.pl "${inside}/${D}"
  printf "# %s %s.cpgIslands.html\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/cpgDescription.pl "${inside}/${D}"
  printf "# %s %s.gc5Base.html\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/gc5Description.pl "${inside}/${D}"
  printf "# %s %s.repeatMasker.html\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/rmskDescription.pl "${inside}/${D}"
  printf "# %s %s.ncbiGene.html\n" "${dateStamp}" "${B}" 1>&2
  ${scripts}/ncbiGeneDescription.pl "${inside}/${D}"

fi

exit 0
