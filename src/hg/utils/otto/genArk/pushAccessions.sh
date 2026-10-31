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
###  By default only the contrib/ directories and hub.txt go out, for
###  assemblies that are already on the destination machines (checked
###  before anything is pushed).  -fullPush also sends the whole assembly
###  directory.  Per accession, in order:
###   1. (-fullPush only) rsync /gbdb/genark/<path>/ hgwdev -> hgwbeta,
###      excluding hub.txt itself plus all the alpha/beta/public/user.hub.txt
###      variants and contrib/ (same idea as pushNewOnes.sh's sendTo(), but
###      this also excludes plain hub.txt -- see pushHubTxt() below for why)
###   2. rsync only the contrib/<track>/ directories whose track name is
###      listed in kent/src/hg/makeDb/trackDb/betaGenArk.txt
###   3. rsync the correct hub.txt to hgwbeta: beta.hub.txt if this
###      accession has one (renamed to hub.txt), otherwise the plain
###      hub.txt as-is -- some assemblies have no alpha/beta/public tiering
###      set up and just carry a single hub.txt (see the "should be fixed
###      up with a symLink" comment in pushNewOnes.sh's sendTo())
###
###  With -rr, after the hgwbeta steps above, the same accession also
###  goes out to the RR machines:
###   4. (-fullPush only) the content files from step 1, rsync from hgwdev
###      to the rrMachList machines, without --delete
###   5. the contrib/<track>/ directories listed in publicGenArk.txt,
###      rsync from hgwdev to the rrMachList machines
###   6. public.hub.txt renamed to hub.txt, or for an assembly with only a
###      plain hub.txt that hub.txt, rsync from hgwdev to the rrMachList
###      machines.  hub.txt goes last so the assembly is not visible on
###      the RR before its files are there.
###
###  The release lists betaGenArk.txt and publicGenArk.txt are read from
###  the otto kent tree, the same tree ottoBuildGenArkHub.py builds from.
###
###  Without -verbose only one line at the start and end of each accession
###  is printed, plus any errors.
###
###  usage: pushAccessions.sh [-rr] [-fullPush] [-verbose] GCA_123456789.1 [GCF_987654321.2 ...]
#############################################################################

set -beEu -o pipefail

export expectHost="hgwdev"
export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

function usage() {
  cat <<EOF 1>&2
usage: $(basename "$0") [-rr] [-fullPush] [-verbose] GCA_123456789.1 [GCF_987654321.2 ...]
  -rr       : also push to the RR machines after hgwbeta
  -fullPush : push the whole assembly directory, not only
              the contrib/ directories and hub.txt
  -verbose  : show each step, every file changed, rsync stats and times
EOF
}

export rrPush="no"
export fullPush="no"
export verbose="no"
while [ $# -gt 0 ] && [[ "${1}" == -* ]]
do
  case "${1}" in
    -rr) rrPush="yes" ;;
    -fullPush) fullPush="yes" ;;
    -verbose) verbose="yes" ;;
    *) printf "ERROR: unknown option '%s'\n" "${1}" 1>&2
       usage
       exit 255 ;;
  esac
  shift
done

if [ $# -lt 1 ]; then
  usage
  exit 255
fi

export destMach="hgwbeta"
export gbdbTop="/gbdb/genark"
export ottoKentTree="/hive/data/outside/genark/ottoKent/kent"
export betaGenArkList="${ottoKentTree}/src/hg/makeDb/trackDb/betaGenArk.txt"
export publicGenArkList="${ottoKentTree}/src/hg/makeDb/trackDb/publicGenArk.txt"
# where content, public.hub.txt and public contrib/ go, as qateam from hgwdev.
# Not the asia node: qateam can not log in there, the cluster
# administrators manage its contents
export rrMachList="hgw0 hgw1 hgw2 genome-euro"
# shared with pushContrib.sh, see there for why it is in /dev/shm
export lockFile="/dev/shm/pushGenArk.lock"

##############################################################################
### only one pushContrib.sh or pushAccessions.sh at a time.  When
### pushContrib.sh runs this script, descriptor 9 is already open on the
### lock file it holds; flock on that inherited descriptor succeeds since
### it is the same open file, while opening the file again would conflict
### with the parent's lock.
##############################################################################
if [ "$(readlink /proc/$$/fd/9 2>/dev/null || true)" != "$(readlink -f "${lockFile}")" ]; then
  [ -e "${lockFile}" ] || : >> "${lockFile}"
  exec 9<"${lockFile}"
fi
if ! flock -n 9; then
  printf "ERROR: another pushContrib.sh or pushAccessions.sh is already running (%s is locked)\n" "${lockFile}" 1>&2
  exit 255
fi

##############################################################################
### rsync with the given arguments; with -verbose also list every file
### changed (--itemize-changes), the transfer --stats and the time taken
##############################################################################
function runRsync() {
  if [ "${verbose}" = "yes" ]; then
    time (rsync --stats --itemize-changes "$@")
  else
    rsync "$@"
  fi
}

##############################################################################
### printf only with -verbose
##############################################################################
function verboseLog() {
  if [ "${verbose}" = "yes" ]; then
    printf "$@"
  fi
}

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
### given a machine name and accession paths, echo the accession paths
### that do not have their <acc>.2bit on that machine, in one ssh call
##############################################################################
function missingOnMach() {
  local mach="${1}"
  shift
  printf "%s\n" "$@" | ssh qateam@${mach} "cd '${gbdbTop}' && while read -r d; do [ -s \"\${d}/\${d##*/}.2bit\" ] || printf '%s\n' \"\${d}\"; done"
}

##############################################################################
### push the ordinary content files for one accession, excluding hub.txt,
### all the tiered hub.txt variants, and contrib/ -- hub.txt handling is
### entirely owned by pushHubTxt() below, not this rsync
##############################################################################
function pushContent() {
  local accDir="${1}"
  verboseLog "### content: %s -> %s %s\n" "${accDir}" "${destMach}" "$(date '+%F %T')"
  ssh qateam@${destMach} "umask 002; mkdir -p '${gbdbTop}/${accDir}'" < /dev/null
  # --delete only removes files rsync would otherwise have considered
  # transferring; excluded paths (contrib/ and the hub.txt files) are
  # excluded from the delete pass too unless --delete-excluded is added.
  # Do NOT add --delete-excluded here -- contrib/ and hub.txt on hgwbeta
  # are owned by pushContrib()/pushHubTxt() below, not this rsync.
  runRsync --delete -a --no-g -L \
    --exclude="hub.txt" \
    --exclude="alpha.hub.txt" --exclude="beta.hub.txt" \
    --exclude="public.hub.txt" --exclude="user.hub.txt" \
    --exclude="contrib/" \
    "${gbdbTop}/${accDir}/" "qateam@${destMach}:${gbdbTop}/${accDir}/"
}

##############################################################################
### push only the contrib/<track>/ directories whose track name is in
### the given release list (betaGenArk.txt or publicGenArk.txt), for one
### accession, to each of the given machines
##############################################################################
function pushContrib() {
  local accDir="${1}"
  local listFile="${2}"
  local machList="${3}"
  if [ ! -s "${listFile}" ]; then
    printf "# no %s found, nothing to do for contrib\n" "${listFile}" 1>&2
    return
  fi
  # grep exits 1 when every line is a comment; with pipefail + set -e
  # that would abort the whole script before pushHubTxt() runs
  { grep -v '^#' "${listFile}" || true; } | while read -r track
  do
    [ -z "${track}" ] && continue
    local contribDir="${gbdbTop}/${accDir}/contrib/${track}"
    if [ -d "${contribDir}" ]; then
      for M in ${machList}
      do
        verboseLog "### contrib: %s -> %s %s\n" "${contribDir}" "${M}" "$(date '+%F %T')"
        runRsync --mkpath -a --no-g -L \
          "${contribDir}/" "qateam@${M}:${contribDir}/"
      done
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
  verboseLog "### hub.txt: %s -> %s:%s %s\n" "${src}" "${destMach}" "${dest}" "$(date '+%F %T')"
  runRsync -a --no-g -L "${src}" "qateam@${destMach}:${dest}"
}

##############################################################################
### push the content files of one accession from hgwdev to the RR, with
### the same excludes as pushContent(); no --delete on the RR, files
### removed on hgwdev stay on the RR machines
##############################################################################
function pushContentRR() {
  local accDir="${1}"
  for M in ${rrMachList}
  do
    verboseLog "### RR content: %s -> %s %s\n" "${accDir}" "${M}" "$(date '+%F %T')"
    runRsync --mkpath -a --no-g -L \
      --exclude="hub.txt" \
      --exclude="alpha.hub.txt" --exclude="beta.hub.txt" \
      --exclude="public.hub.txt" --exclude="user.hub.txt" \
      --exclude="contrib/" \
      "${gbdbTop}/${accDir}/" "qateam@${M}:${gbdbTop}/${accDir}/"
  done
}

##############################################################################
### push the public hub.txt for this accession to the rrMachList machines:
###   public.hub.txt -> hub.txt
###   beta.hub.txt but no public.hub.txt -> not public yet, no hub.txt
###   only a plain hub.txt -> that same hub.txt
##############################################################################
function pushHubTxtRR() {
  local accDir="${1}"
  local publicSrc="${gbdbTop}/${accDir}/public.hub.txt"
  local betaSrc="${gbdbTop}/${accDir}/beta.hub.txt"
  local plainSrc="${gbdbTop}/${accDir}/hub.txt"
  local dest="${gbdbTop}/${accDir}/hub.txt"
  local src=""
  if [ -s "${publicSrc}" ]; then
    src="${publicSrc}"
  elif [ -s "${betaSrc}" ]; then
    printf "# %s has beta.hub.txt but no public.hub.txt, no hub.txt to the RR\n" "${accDir}" 1>&2
    return
  elif [ -s "${plainSrc}" ]; then
    src="${plainSrc}"
  else
    printf "ERROR: no public.hub.txt or hub.txt found for %s\n" "${accDir}" 1>&2
    exit 255
  fi
  for M in ${rrMachList}
  do
    verboseLog "### RR hub.txt: %s -> %s:%s %s\n" "${src}" "${M}" "${dest}" "$(date '+%F %T')"
    runRsync -a --no-g -L "${src}" "qateam@${M}:${dest}"
  done
}

##############################################################################
### main
##############################################################################
accDirList=()
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
  accDirList+=("${accDir}")
done

### without -fullPush, every assembly must already be on every destination,
### otherwise a hub.txt would go out with no assembly files behind it
if [ "${fullPush}" = "no" ]; then
  checkList="${destMach}"
  [ "${rrPush}" = "yes" ] && checkList="${checkList} ${rrMachList}"
  missingCount=0
  for M in ${checkList}
  do
    missing=$(missingOnMach "${M}" "${accDirList[@]}")
    if [ -n "${missing}" ]; then
      printf "ERROR: not on %s, use -fullPush to send the whole assembly:\n" "${M}" 1>&2
      printf "%s\n" "${missing}" 1>&2
      missingCount=$((missingCount + 1))
    fi
  done
  if [ "${missingCount}" -gt 0 ]; then
    exit 255
  fi
fi

for accDir in "${accDirList[@]}"
do
  printf "#### pushing %s %s\n" "${accDir}" "$(date '+%F %T')"
  [ "${fullPush}" = "yes" ] && pushContent "${accDir}"
  pushContrib "${accDir}" "${betaGenArkList}" "${destMach}"
  pushHubTxt "${accDir}"
  if [ "${rrPush}" = "yes" ]; then
    [ "${fullPush}" = "yes" ] && pushContentRR "${accDir}"
    pushContrib "${accDir}" "${publicGenArkList}" "${rrMachList}"
    pushHubTxtRR "${accDir}"
  fi
  printf "#### finished %s %s\n" "${accDir}" "$(date '+%F %T')"
done
