#!/bin/bash

export TOP="/hive/data/outside/genark/RCPediaVGP_v1"

export contribName="RCPediaVGP_v1"
# set -x

export asmList=""

for D in $(ls -d RCPediaVGP_v1/GC* | head -5)
do
  if [ ! -d "${D}" ]; then
    continue
  fi
  acc=`basename $D`
  gcX="${acc:0:3}"
  d0="${acc:4:3}"
  d1="${acc:7:3}"
  d2="${acc:10:3}"
  P="${gcX}/${d0}/${d1}/${d2}/${acc}"
  printf "# working: %s\n" "${P}" 1>&2
  aB="genbankBuild"
  if [ "${gcX}" = "GCF" ]; then
    aB="refseqBuild"
  fi
  buildPath=`ls -d /hive/data/genomes/asmHubs/$aB/${P}*`
  if [ -d "${buildPath}" ]; then
     mkdir -p "${buildPath}/contrib/${contribName}"
#     ls -ld "${buildPath}/contrib/${contribName}"
     while read -r fileName; do
       B=`basename "${fileName}"`
       rm -f "${buildPath}/contrib/${contribName}/retrocopies.bb"
       printf "ln -s %s/%s %s/contrib/%s/retrocopies.bb\n" "${TOP}" "${fileName}" "${buildPath}" "${contribName}"
       ln -s "${TOP}/${fileName}" "${buildPath}/contrib/${contribName}/retrocopies.bb"
     done < <(find ./${D} -type f | grep "myTrack.*.bb" | sed -e 's#^./##;')
     rm -f "${buildPath}/contrib/${contribName}/retrocopies.html"
     rm -f "${buildPath}/contrib/${contribName}/RCPediaVGP_v1.trackDb.txt"
     ln -s "${TOP}/RCPediaVGP_v1.html" "${buildPath}/contrib/${contribName}/retrocopies.html"
     ln -s "${TOP}/RCPediaVGP_v1.trackDb.txt" "${buildPath}/contrib/${contribName}/RCPediaVGP_v1.trackDb.txt"
     asmList+=" ${acc}"
  else
     printf "ERROR: Not found:\n%s\n" "${buildPath}" 1>&2
  fi
  printf "# end done: %s\n" "${buildPath}" 1>&2
done

printf "ottoBuildGenArkHub.py ${asmList}\n"
