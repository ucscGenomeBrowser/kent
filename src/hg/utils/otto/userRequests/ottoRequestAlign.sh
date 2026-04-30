#!/bin/bash

# ottoRequestAlign.sh - look up an ottoRequest row by id and construct
#   the kegAlignLastz.sh command line from genark metadata
#
# usage: ottoRequestAlign.sh <id>
#
# Queries hgcentraltest.ottoRequest for fromDb/toDb, then looks up
# each accession in hgcentraltest.genark for asmName and clade.
# Prints and executes the resulting kegAlignLastz.sh command.

set -eEu -o pipefail
set -x

############################################################################
### verify arguments
############################################################################
if [ $# != 1 ]; then
  printf "usage: ottoRequestAlign.sh <id>\n" 1>&2
  printf "  where <id> is a row id from hgcentraltest.ottoRequest\n" 1>&2
  exit 255
fi

export requestId="$1"

# validate id is a positive integer
case "${requestId}" in
  ''|*[!0-9]*)
    printf "ERROR: id must be a positive integer, got: '%s'\n" "${requestId}" 1>&2
    exit 255
    ;;
esac

############################################################################
### function definitions
############################################################################
### errors - set error status in the table
function setErrorStatus() {
  id="${1}"
  hgsql -N -e \
      "UPDATE ottoRequest SET status=7 WHERE id=${id};" hgcentraltest
}

############################################################################
# genarkLookup - query genark table for accession, asmName, clade
#   arg: gcAccession (e.g. GCF_000002285.3)
#   sets: _acc, _asmName, _clade
############################################################################
function genarkLookup() {
  local acc=$1
  local result=$(hgsql -N -e \
    "SELECT gcAccession,asmName,clade from genark WHERE gcAccession='${acc}';" \
    hgcentraltest)
  if [ -z "${result}" ]; then
    printf "ERROR: accession '%s' not found in hgcentraltest.genark\n" "${acc}" 1>&2
    setErrorStatus ${requestId}
    return 1
  fi
  _acc=$(printf "%s" "${result}" | cut -f1)
  _asmName=$(printf "%s" "${result}" | cut -f2)
  _clade=$(printf "%s" "${result}" | cut -f3)
}

############################################################################
# dbDbCladeLookup - look up clade for a UCSC database name
#   from dbDb.name.clade.tsv (in same directory as this script)
#   arg: dbName (e.g. hg38, rn7)
#   sets: _clade
############################################################################
function dbDbCladeLookup() {
  local dbName=$1
  local scriptDir=$(cd "$(dirname "$0")" && pwd)
  local tsvFile="${scriptDir}/dbDb.name.clade.tsv"
  if [ ! -s "${tsvFile}" ]; then
    printf "ERROR: clade lookup file not found: %s\n" "${tsvFile}" 1>&2
    return 1
  fi
  _clade=$(grep -v '^#' "${tsvFile}" | awk -F'\t' -v db="${dbName}" '$1==db {print $2}')
  if [ -z "${_clade}" ]; then
    printf "ERROR: UCSC db '%s' not found in %s\n" "${dbName}" "${tsvFile}" 1>&2
    return 1
  fi
}

############################################################################
# cladeMap - convert genark/dbDb plural clade to kegAlignLastz singular form
#   primates -> primate, mammals -> mammal, everything else -> other
############################################################################
function cladeMap() {
  local genarkClade=$1
  case "${genarkClade}" in
    primates) printf "primate" ;;
    mammals)  printf "mammal"  ;;
    *)        printf "other"   ;;
  esac
}

############################################################################
# twoBitPath - return path to 2bit file
############################################################################
function twoBitPath() {
  local asmName=$1
  case ${asmName} in
    GC[AF]_*)
      local gcX=$(printf "%s" "${asmName}" | cut -c1-3)
      local d0=$(printf "%s" "${asmName}" | cut -c5-7)
      local d1=$(printf "%s" "${asmName}" | cut -c8-10)
      local d2=$(printf "%s" "${asmName}" | cut -c11-13)
      printf "/hive/data/genomes/asmHubs/%s/%s/%s/%s/%s/%s.2bit" \
        "${gcX}" "${d0}" "${d1}" "${d2}" "${asmName}" "${asmName}"
      ;;
    *)
      printf "/hive/data/genomes/%s/%s.2bit" "${asmName}" "${asmName}"
      ;;
  esac
}
############################################################################

############################################################################
# asmN50 - compute N50 from the twoBit file
############################################################################
function asmN50() {
  local twoBit=$1
  twoBitInfo "${twoBit}" stdout \
    | n50.pl stdin 2>&1 \
    | grep -A1 "^[0-9].*one half size" \
    | tail -1 \
    | awk '{print $NF}'
}
############################################################################

############################################################################
### main() scripting begins here
############################################################################

############################################################################
# step 1: look up fromDb and toDb from ottoRequest
############################################################################
export ottoResult=$(hgsql -N -e \
  "SELECT fromDb,toDb from ottoRequest WHERE id=${requestId} AND status = 1 AND requestType = 'liftOver';" hgcentraltest)

if [ -z "${ottoResult}" ]; then
  printf "ERROR: no ottoRequest row found for id=%s AND status = 1\n" "${requestId}" 1>&2
  hgsql -e "SELECT fromDb,toDb,status from ottoRequest WHERE id=${requestId};" hgcentraltest 1>&2
  exit 255
fi

export fromDb=$(printf "%s" "${ottoResult}" | cut -f1)
export toDb=$(printf "%s" "${ottoResult}" | cut -f2)

if [ -z "${fromDb}" -o -z "${toDb}" ]; then
  printf "ERROR: empty fromDb or toDb for ottoRequest id=%s\n" "${requestId}" 1>&2
  printf "  got: fromDb='%s' toDb='%s'\n" "${fromDb}" "${toDb}" 1>&2
  setErrorStatus ${requestId}
  exit 255
fi

printf "# ottoRequest id=%s: fromDb='%s' toDb='%s'\n" \
  "${requestId}" "${fromDb}" "${toDb}" 1>&2

############################################################################
# step 2: look up both identifiers -- GenArk accession or UCSC db name
############################################################################
case "${fromDb}" in
  GC[AF]_*)
    genarkLookup "${fromDb}" || exit 255
    export fromId="${_acc}_${_asmName}"
    export fromClade="${_clade}"
    ;;
  *)
    dbDbCladeLookup "${fromDb}" || exit 255
    export fromId="${fromDb}"
    export fromClade="${_clade}"
    ;;
esac

case "${toDb}" in
  GC[AF]_*)
    genarkLookup "${toDb}" || exit 255
    export toId="${_acc}_${_asmName}"
    export toClade="${_clade}"
    ;;
  *)
    dbDbCladeLookup "${toDb}" || exit 255
    export toId="${toDb}"
    export toClade="${_clade}"
    ;;
esac

printf "# from: %s  clade=%s\n" "${fromId}" "${fromClade}" 1>&2
printf "#   to: %s  clade=%s\n" "${toId}" "${toClade}" 1>&2

############################################################################
# step 3: determine N50 for each to decide target vs. query
############################################################################

export from2bit=$(twoBitPath "${fromDb}")
export to2bit=$(twoBitPath "${toDb}")

if [ ! -s "${from2bit}" ]; then
  printf "ERROR: 2bit file not found: %s\n" "${from2bit}" 1>&2
  setErrorStatus ${requestId}
  exit 255
fi

if [ ! -s "${to2bit}" ]; then
  printf "ERROR: 2bit file not found: %s\n" "${to2bit}" 1>&2
  setErrorStatus ${requestId}
  exit 255
fi

export fromN50=$(asmN50 "${from2bit}")
export toN50=$(asmN50 "${to2bit}")

printf "# from N50: %s (%s)\n" "${fromN50}" "${fromDb}" 1>&2
printf "#   to N50: %s (%s)\n" "${toN50}" "${toDb}" 1>&2

if [ -n "${fromN50}" -a -n "${toN50}" ]; then
  if [ "${toN50}" -gt "${fromN50}" ]; then
    printf "# swapping: %s (N50=%s) becomes target over %s (N50=%s)\n" \
      "${toId}" "${toN50}" "${fromId}" "${fromN50}" 1>&2
    tmpId="${fromId}"; export fromId="${toId}"; export toId="${tmpId}"
    tmpClade="${fromClade}"; export fromClade="${toClade}"; export toClade="${tmpClade}"
  fi
else
  printf "WARNING: could not determine N50, keeping original target/query order\n" 1>&2
fi

############################################################################
# step 4: compute buildDir and update ottoRequest table
############################################################################

# derive accession ID and Query label the same way kegAlignLastz.sh does
export tAccId=$(printf "%s" "${fromId}" | cut -d'_' -f1-2)
export qAccId=$(printf "%s" "${toId}" | cut -d'_' -f1-2)
export Query="${qAccId^}"
export DS=$(date "+%F")

# compute buildDir -- UCSC db default, then GenArk override
export buildDir="/hive/data/genomes/${fromId}/bed/lastz${Query}.${DS}"
export targetExists="/hive/data/genomes/${fromId}/bed"

case ${fromId} in
  GC[AF]_*)
    gcX=$(printf "%s" "${fromId}" | cut -c1-3)
    d0=$(printf "%s" "${fromId}" | cut -c5-7)
    d1=$(printf "%s" "${fromId}" | cut -c8-10)
    d2=$(printf "%s" "${fromId}" | cut -c11-13)
    tGcPath="${gcX}/${d0}/${d1}/${d2}"
    buildDir="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${fromId}/trackData/lastz${Query}.${DS}"
    targetExists="/hive/data/genomes/asmHubs/allBuild/${tGcPath}/${fromId}/trackData"
    ;;
esac

# reuse existing build directory if one is already in progress
working=$(ls -d ${targetExists}/lastz${Query}.* 2> /dev/null | wc -l || true)
if [ "${working}" -gt 0 ]; then
  buildDir=$(ls -d ${targetExists}/lastz${Query}.* | tail -1)
  printf "# existing buildDir: %s\n" "${buildDir}" 1>&2
fi

mkdir -p  "${buildDir}"

printf "# buildDir: %s\n" "${buildDir}" 1>&2

# store buildDir in ottoRequest table for workflowMonitor.sh
hgsql -N -e \
  "UPDATE ottoRequest SET buildDir='${buildDir}', status=2 WHERE id=${requestId};" \
  hgcentraltest

############################################################################
# step 5: map clades and build the kegAlignLastz.sh command
############################################################################
export fromCladeArg=$(cladeMap "${fromClade}")
export toCladeArg=$(cladeMap "${toClade}")

export cmd="${HOME}/kent/src/hg/utils/automation/kegAlignLastz.sh ${fromId} ${toId} ${fromCladeArg} ${toCladeArg}"

printf "# launching: %s\n" "${cmd}" 1>&2
nohup ${cmd} > "${buildDir}/kegAlign.log" 2>&1 < /dev/null &
printf "# launched pid %s, log=%s/kegAlign.log\n" "$!" "${buildDir}" 1>&2
exit 0
