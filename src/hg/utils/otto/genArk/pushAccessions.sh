#!/bin/bash

#############################################################################
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/pushAccessions.sh
###
###  On-demand, single-or-few-accession version of the nightly pushRR.sh
###  pipeline.  Given one or more GenArk accessions, pushes just that
###  content from hgwdev to hgwbeta -- skips the whole-tree
###  find/stat inventory scan (devList.sh + whatIsNew.sh) entirely, since
###  the accession(s) to push are already known by the caller.
###
###  Per accession, in order:
###   1. rsync /gbdb/genark/<path>/ hgwdev -> hgwbeta, excluding hub.txt
###      itself plus all the alpha/beta/public/user.hub.txt variants and
###      contrib/ (same idea as pushNewOnes.sh's sendTo(), but this also
###      excludes plain hub.txt -- see pushHubTxt() below for why)
###   2. rsync only the contrib/<track>/ directories whose track name is
###      listed in kent/src/hg/makeDb/trackDb/betaGenArk.txt
###   3. rsync the correct hub.txt to hgwbeta: beta.hub.txt if this
###      accession has one (renamed to hub.txt), otherwise the plain
###      hub.txt as-is -- some assemblies have no alpha/beta/public tiering
###      set up and just carry a single hub.txt (see the "should be fixed
###      up with a symLink" comment in pushNewOnes.sh's sendTo())
###
###  This only ever reaches hgwbeta.  Promoting to the public RR mirrors
###  is still the separate cluster-admin pushFromBetaToRR.sh step.
###
###  usage: pushAccessions.sh GCA_123456789.1 [GCF_987654321.2 ...]
#############################################################################

set -beEu -o pipefail

export expectHost="hgwdev"
export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

if [ $# -lt 1 ]; then
  printf "usage: %s GCA_123456789.1 [GCF_987654321.2 ...]\n" "$(basename "$0")" 1>&2
  exit 255
fi

export destMach="hgwbeta"
export gbdbTop="/gbdb/genark"
export betaGenArkList="${HOME}/kent/src/hg/makeDb/trackDb/betaGenArk.txt"

##############################################################################
### given an accession, echo its /gbdb/genark path, e.g.
###   GCA_123456789.1  ->  GCA/123/456/789/GCA_123456789.1
##############################################################################
function accessionToPath() {
  local acc="${1}"
  local prefix="${acc:0:3}"
  local digits="${acc:4:9}"
  printf "%s/%s/%s/%s/%s\n" "${prefix}" "${digits:0:3}" "${digits:3:3}" "${digits:6:3}" "${acc}"
}

##############################################################################
### push the ordinary content files for one accession, excluding hub.txt,
### all the tiered hub.txt variants, and contrib/ -- hub.txt handling is
### entirely owned by pushHubTxt() below, not this rsync
##############################################################################
function pushContent() {
  local accDir="${1}"
  printf "### content: %s -> %s %s\n" "${accDir}" "${destMach}" "$(date '+%F %T')"
  ssh qateam@${destMach} "umask 002; mkdir -p '${gbdbTop}/${accDir}'" < /dev/null
  # --delete only removes files rsync would otherwise have considered
  # transferring; excluded paths (contrib/ and the hub.txt files) are
  # excluded from the delete pass too unless --delete-excluded is added.
  # Do NOT add --delete-excluded here -- contrib/ and hub.txt on hgwbeta
  # are owned by pushContrib()/pushHubTxt() below, not this rsync.
  time (rsync --delete --stats -a -L --itemize-changes \
    --exclude="hub.txt" \
    --exclude="alpha.hub.txt" --exclude="beta.hub.txt" \
    --exclude="public.hub.txt" --exclude="user.hub.txt" \
    --exclude="contrib/" \
    "${gbdbTop}/${accDir}/" "qateam@${destMach}:${gbdbTop}/${accDir}/")
}

##############################################################################
### push only the contrib/<track>/ directories that are on the beta
### release list (betaGenArk.txt), for one accession
##############################################################################
function pushContrib() {
  local accDir="${1}"
  if [ ! -s "${betaGenArkList}" ]; then
    printf "# no betaGenArk.txt found, nothing to do for contrib\n" 1>&2
    return
  fi
  grep -v '^#' "${betaGenArkList}" | while read -r track
  do
    [ -z "${track}" ] && continue
    local contribDir="${gbdbTop}/${accDir}/contrib/${track}"
    if [ -d "${contribDir}" ]; then
      printf "### contrib: %s -> %s %s\n" "${contribDir}" "${destMach}" "$(date '+%F %T')"
      time (rsync --mkpath --stats -a -L --itemize-changes \
        "${contribDir}/" "qateam@${destMach}:${contribDir}/")
    fi
  done
}

##############################################################################
### push the correct hub.txt for this accession: beta.hub.txt renamed to
### hub.txt when it exists (the tiered/staged case); otherwise fall back
### to the plain hub.txt as-is, for assemblies with no beta tier set up
##############################################################################
function pushHubTxt() {
  local accDir="${1}"
  local betaSrc="${gbdbTop}/${accDir}/beta.hub.txt"
  local plainSrc="${gbdbTop}/${accDir}/hub.txt"
  local dest="${gbdbTop}/${accDir}/hub.txt"
  local src=""
  if [ -s "${betaSrc}" ]; then
    src="${betaSrc}"
  elif [ -s "${plainSrc}" ]; then
    src="${plainSrc}"
  else
    printf "ERROR: no beta.hub.txt or hub.txt found for %s\n" "${accDir}" 1>&2
    exit 255
  fi
  printf "### hub.txt: %s -> %s:%s %s\n" "${src}" "${destMach}" "${dest}" "$(date '+%F %T')"
  time (rsync --stats -a -L --itemize-changes "${src}" "qateam@${destMach}:${dest}")
}

##############################################################################
### main
##############################################################################
for acc in "$@"
do
  if [[ ! "${acc}" =~ ^GC[AF]_[0-9]{9}\.[0-9]+$ ]]; then
    printf "ERROR: '%s' does not look like a GenArk accession (GCA_123456789.1)\n" "${acc}" 1>&2
    exit 255
  fi
  accDir=$(accessionToPath "${acc}")
  if [ ! -d "${gbdbTop}/${accDir}" ]; then
    printf "ERROR: no such directory %s/%s\n" "${gbdbTop}" "${accDir}" 1>&2
    exit 255
  fi
  printf "#### pushing %s (%s) %s\n" "${acc}" "${accDir}" "$(date '+%F %T')"
  pushContent "${accDir}"
  pushContrib "${accDir}"
  pushHubTxt "${accDir}"
  printf "#### finished %s %s\n" "${acc}" "$(date '+%F %T')"
done
