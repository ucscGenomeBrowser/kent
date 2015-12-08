#!/bin/bash

# ucscPatchAltsFa.sh - process the UCSC patch sequences and alternates into
#   fasta files from the ncbi.2bit file

# fail on any error:
set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "%s\n" "usage: ucscPatchAlts.sh </fullPathTo/inside/assemblyDirectory/>" 1>&2
  printf "%s\n" "builds UCSC fasta files for patch and alternate sequences" 1>&2
  exit 255
fi

export dateStamp=`date "+%FT%T %s"`

export asmDirectory=$1
export asmName=`basename "${asmDirectory}"`

export ncbi2Bit="${asmDirectory}/${asmName}.ncbi.2bit"

if [ ! -s "${ncbi2Bit}" ]; then
  printf "# %s ERROR: ucscPatchAltsFa.sh can not find 2bit file %s\n" \
     "${dateStamp}" "${ncbi2Bit}" 1>&2
fi
###
### patch sequences first
###
if [ -s "${asmDirectory}/${asmName}.ucsc.to.ncbi.patch.names" ]; then
   if [ "${asmDirectory}/${asmName}.ucsc.to.ncbi.patch.names" -nt "${asmDirectory}/${asmName}.patch.fa.gz" ]; then
     printf "# %s building %s.patch.fa.gz\n" "${dateStamp}" "${asmName}" 1>&2
     awk '{printf "s/^>%s/>%s/;\n", $2, $1}' \
      "${asmDirectory}/${asmName}.ucsc.to.ncbi.patch.names" \
        > ${asmDirectory}/ncbi.to.ucsc.patch.sed
     cut -f2 "${asmDirectory}/${asmName}.ucsc.to.ncbi.patch.names" \
        | twoBitToFa -seqList=stdin ${ncbi2Bit} stdout \
          | sed -f ${asmDirectory}/ncbi.to.ucsc.patch.sed | gzip -c \
            > "${asmDirectory}/${asmName}.patch.fa.gz"
     touch -r "${asmDirectory}/${asmName}.ucsc.to.ncbi.patch.names" \
        "${asmDirectory}/${asmName}.patch.fa.gz"
     rm -f ${asmDirectory}/ncbi.to.ucsc.patch.sed
   fi
fi
###
### alternate sequences second
###
if [ -s "${asmDirectory}/${asmName}.ucsc.to.ncbi.alt.names" ]; then
   if [ "${asmDirectory}/${asmName}.ucsc.to.ncbi.alt.names" -nt "${asmDirectory}/${asmName}.alt.fa.gz" ]; then
     printf "# %s building %s.alt.fa.gz\n" "${dateStamp}" "${asmName}" 1>&2
     awk '{printf "s/^>%s/>%s/;\n", $2, $1}' \
      "${asmDirectory}/${asmName}.ucsc.to.ncbi.alt.names" \
          > ${asmDirectory}/ncbi.to.ucsc.alt.sed
     cut -f2 "${asmDirectory}/${asmName}.ucsc.to.ncbi.alt.names" \
        | twoBitToFa -seqList=stdin ${ncbi2Bit} stdout \
          | sed -f ${asmDirectory}/ncbi.to.ucsc.alt.sed | gzip -c \
            > "${asmDirectory}/${asmName}.alt.fa.gz"
     touch -r "${asmDirectory}/${asmName}.ucsc.to.ncbi.alt.names" \
         "${asmDirectory}/${asmName}.alt.fa.gz"
     rm -f ${asmDirectory}/ncbi.to.ucsc.alt.sed
   fi
fi
