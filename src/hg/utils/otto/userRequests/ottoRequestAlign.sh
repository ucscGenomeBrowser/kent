#!/bin/bash

# ottoRequestAlign.sh - look up an ottoRequest row by id and construct
#   the kegAlignLastz.sh command line from genark metadata
#
# usage: ottoRequestAlign.sh <id>
#
# Queries hgcentraltest.ottoRequest for fromDb/toDb, then looks up
# each accession in hgcentraltest.genark for asmName and clade.
# Prints and executes the resulting kegAlignLastz.sh command.

set -beEu -o pipefail

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
# step 1: look up fromDb and toDb from ottoRequest
############################################################################
export ottoResult=$(hgsql -N -e \
  "select fromDb,toDb from ottoRequest where id=${requestId};" hgcentraltest)

if [ -z "${ottoResult}" ]; then
  printf "ERROR: no ottoRequest row found for id=%s\n" "${requestId}" 1>&2
  exit 255
fi

export fromDb=$(printf "%s" "${ottoResult}" | cut -f1)
export toDb=$(printf "%s" "${ottoResult}" | cut -f2)

if [ -z "${fromDb}" -o -z "${toDb}" ]; then
  printf "ERROR: empty fromDb or toDb for ottoRequest id=%s\n" "${requestId}" 1>&2
  printf "  got: fromDb='%s' toDb='%s'\n" "${fromDb}" "${toDb}" 1>&2
  exit 255
fi

printf "# ottoRequest id=%s: fromDb='%s' toDb='%s'\n" \
  "${requestId}" "${fromDb}" "${toDb}" 1>&2

############################################################################
# genarkLookup - query genark table for accession, asmName, clade
#   arg: gcAccession (e.g. GCF_000002285.3)
#   sets: _acc, _asmName, _clade
############################################################################
function genarkLookup() {
  local acc=$1
  local result=$(hgsql -N -e \
    "select gcAccession,asmName,clade from genark where gcAccession='${acc}';" \
    hgcentraltest)
  if [ -z "${result}" ]; then
    printf "ERROR: accession '%s' not found in hgcentraltest.genark\n" "${acc}" 1>&2
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
# step 3: map clades and build the command
############################################################################
export fromCladeArg=$(cladeMap "${fromClade}")
export toCladeArg=$(cladeMap "${toClade}")

export cmd="kegAlignLastz.sh ${fromId} ${toId} ${fromCladeArg} ${toCladeArg}"

printf "# %s\n" "${cmd}" 1>&2
