#!/bin/bash

# cpg.sh - process *.ncbi.2bit file into CpG Islands tracks, both on unmasked
#         sequence and on masked sequence

# fail on any error:
set -beEu -o pipefail

if [ $# -ne 1 ]; then
  printf "%s\n" "usage: cpg.sh </fullPathTo/inside/assemblyDirectory/>" 1>&2
  printf "%s\n" "builds CpG Islands tracks, both unmasked and masked" 1>&2
  exit 255
fi

export dateStamp=`date "+%FT%T %s"`

export asmDirectory=$1
export asmName=`basename "${asmDirectory}"`

cd "${asmDirectory}"
# could be a zero length file, which means there are no results to be had
if [ ! -f "bbi/${asmName}.cpgIslandExtUnmasked.ncbi.bb" ]; then
  rm -fr cpgU
  sleep 1
  mkdir cpgU
  touch `pwd`/cpgU
  printf "# %s cpgIslandExtUnmasked from %s.ncbi.2bit\n" "${dateStamp}" "${asmName}" 1>&2
  twoBitToFa -noMask "${asmDirectory}/${asmName}.ncbi.2bit" stdout \
    | faToTwoBit stdin "${asmDirectory}/cpgU/ncbi.noMask.2bit"
  /cluster/bin/scripts/doCpgIslands.pl -dbHost=hgwdev -bigClusterHub=ku \
     -buildDir=`pwd`/cpgU \
       -stop=makeBed -tableName=cpgIslandExtUnmasked.ncbi \
         -chromSizes="${asmDirectory}/${asmName}.ncbi.chrom.sizes" \
          -maskedSeq="${asmDirectory}/cpgU/ncbi.noMask.2bit" -workhorse=hgwdev \
              -smallClusterHub=ku "${asmName}" > cpgU/do.log 2>&1
  mv "cpgU/${asmName}.cpgIslandExtUnmasked.ncbi.bb" bbi
#  ssh ku "cd ${asmDirectory}/cpg; para clearSickNodes; para flushResults; para resetCounts; para freeBatch"
  sleep 1
#  rm -f ncbi.noMask.2bit
#  rm -fr cpg
  sleep 1
  touch -r "${asmName}.ncbi.2bit" "bbi/${asmName}.cpgIslandExtUnmasked.ncbi.bb"
else
  printf "# done: %s.cpgIslandExtUnmasked.ncbi.bb\n" "${asmName}" 1>&2
fi
# could be a zero length file, which means there are no results to be had
if [ ! -f "bbi/${asmName}.cpgIslandExt.ncbi.bb" ]; then
  rm -fr cpg
  sleep 1
  mkdir cpg
  touch `pwd`/cpg
  printf "# %s cpgIslandExt from %s.ncbi.2bit\n" "${dateStamp}" "${asmName}" 1>&2
  /cluster/bin/scripts/doCpgIslands.pl -dbHost=hgwdev -bigClusterHub=ku \
     -buildDir=`pwd`/cpg \
       -stop=makeBed -tableName=cpgIslandExt.ncbi \
         -chromSizes="${asmDirectory}/${asmName}.ncbi.chrom.sizes" \
          -maskedSeq="${asmDirectory}/${asmName}.ncbi.2bit" -workhorse=hgwdev \
              -smallClusterHub=ku "${asmName}" > cpg/do.log 2>&1
  mv "cpg/${asmName}.cpgIslandExt.ncbi.bb" bbi
#  ssh ku "cd ${asmDirectory}/cpg; para clearSickNodes; para flushResults; para resetCounts; para freeBatch"
  sleep 1
#  rm -fr cpg
  sleep 1
  touch -r "${asmName}.ncbi.2bit" "bbi/${asmName}.cpgIslandExt.ncbi.bb"
else
  printf "# done: %s.cpgIslandExt.ncbi.bb\n" "${asmName}" 1>&2
fi
