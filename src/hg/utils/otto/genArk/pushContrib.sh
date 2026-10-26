#!/bin/bash

#############################################################################
###  This source is from the source tree:
###     ~/kent/src/hg/utils/otto/genArk/pushContrib.sh
###
###  Release one contrib track collection for every GenArk assembly that
###  has it: 'beta' pushes to hgwbeta, 'public' pushes to hgwbeta and the
###  RR machines.
###
###   1. find the accessions with /contrib/<contribName>/ files in the
###      daily hgwdev listing dev.todayList.gz
###   2. check the release lists in the otto kent tree:
###        beta:   contribName must already be in betaGenArk.txt
###        public: contribName must be in publicGenArk.txt; when it is
###                only in betaGenArk.txt it is added to publicGenArk.txt,
###                committed with 'refs #<redmine>' and pushed to origin.
###                The push must happen before step 3, since
###                ottoBuildGenArkHub.py resets the otto kent tree to
###                origin/master.
###   3. ottoBuildGenArkHub.py rebuilds trackDb.txt, hub.txt,
###      beta.hub.txt and public.hub.txt for those accessions
###   4. pushAccessions.sh pushes them (with -rr for 'public'); by default
###      only the contrib/ directories and hub.txt, with -fullPush the
###      whole assembly directory
###
###  usage: pushContrib.sh [-fullPush] <contribName> <redmine #> <beta|public>
#############################################################################

set -beEu -o pipefail

export expectHost="hgwdev"
export hostName=$(hostname -s)
if [[ "${hostName}" != "${expectHost}" ]]; then
  printf "ERROR: must run this on %s !  This is: %s\n" "${expectHost}" "${hostName}" 1>&2
  exit 255
fi

function usage() {
  printf "usage: %s [-fullPush] <contribName> <redmine #> <beta|public>\n" "$(basename "$0")" 1>&2
  printf "  beta      : push to hgwbeta, contribName must be in betaGenArk.txt\n" 1>&2
  printf "  public    : push to hgwbeta and the RR machines, contribName is\n" 1>&2
  printf "              added to publicGenArk.txt when only in betaGenArk.txt\n" 1>&2
  printf "  -fullPush : push the whole assembly directories, not only\n" 1>&2
  printf "              the contrib/ directories and hub.txt\n" 1>&2
}

export fullPushOpt=""
if [ $# -gt 0 ] && [ "${1}" = "-fullPush" ]; then
  fullPushOpt="-fullPush"
  shift
fi

if [ $# -ne 3 ]; then
  usage
  exit 255
fi

export contribName="${1}"
export redmineNumber="${2}"
export releaseTier="${3}"

if [[ ! "${contribName}" =~ ^[A-Za-z0-9._-]+$ ]]; then
  printf "ERROR: contrib name '%s' must be only letters, digits, '.', '_' or '-'\n" "${contribName}" 1>&2
  exit 255
fi
if [[ ! "${redmineNumber}" =~ ^[0-9]+$ ]]; then
  printf "ERROR: redmine number '%s' must be only digits\n" "${redmineNumber}" 1>&2
  exit 255
fi
if [ "${releaseTier}" != "beta" ] && [ "${releaseTier}" != "public" ]; then
  printf "ERROR: third argument must be 'beta' or 'public', not '%s'\n" "${releaseTier}" 1>&2
  exit 255
fi

export ottoKentTree="/hive/data/outside/genark/ottoKent/kent"
export listDir="src/hg/makeDb/trackDb"
export betaGenArkList="${ottoKentTree}/${listDir}/betaGenArk.txt"
export publicGenArkList="${ottoKentTree}/${listDir}/publicGenArk.txt"
export devTodayList="/hive/data/inside/GenArk/pushRR/dev.todayList.gz"
export ottoBuild="/cluster/bin/scripts/ottoBuildGenArkHub.py"
export scriptDir=$(dirname "$(readlink -f "$0")")
export pushAccessions="${scriptDir}/pushAccessions.sh"
export ottoBuildLock="/cluster/bin/scripts/ottoRequestPush.lock"
# shared with pushAccessions.sh; on local disk, since flock on an NFS file
# is emulated with per-process POSIX locks and a child process could not
# share the lock this script holds
export lockFile="/dev/shm/pushGenArk.lock"

##############################################################################
### only one pushContrib.sh or pushAccessions.sh at a time; the lock is held
### on file descriptor 9 until this script exits, however it exits.
### pushAccessions.sh, run below, inherits descriptor 9 and uses this
### same lock.  Opened read-only so any user can open a lock file
### another user created.
##############################################################################
[ -e "${lockFile}" ] || : >> "${lockFile}"
exec 9<"${lockFile}"
if ! flock -n 9; then
  printf "ERROR: another pushContrib.sh or pushAccessions.sh is already running (%s is locked)\n" "${lockFile}" 1>&2
  exit 255
fi

##############################################################################
### 1. the accessions that have /contrib/<contribName>/ files on hgwdev
##############################################################################
if [ ! -s "${devTodayList}" ]; then
  printf "ERROR: can not find %s\n" "${devTodayList}" 1>&2
  exit 255
fi

export accList=$(mktemp /dev/shm/pushContrib.XXXXXX)
trap 'rm -f "${accList}"' EXIT

# lines are: mtime<tab>GCA/123/456/789/GCA_123456789.1/contrib/<name>/...
# zgrep exits 1 when nothing matches (the empty list is checked below),
# 2 on a read error, e.g. a truncated file while devList.sh's daily
# 'cp -p' is rewriting it in place
export zgrepStatus=0
zgrep -F "/contrib/${contribName}/" "${devTodayList}" > "${accList}" || zgrepStatus=$?
if [ "${zgrepStatus}" -gt 1 ]; then
  printf "ERROR: can not read %s, it may be in the middle of its daily update, try again later\n" "${devTodayList}" 1>&2
  exit 255
fi
cut -f2 "${accList}" | cut -d/ -f5 | sort -u > "${accList}.tmp"
mv "${accList}.tmp" "${accList}"

if grep -v -x -E 'GC[AF]_[0-9]{9}\.[0-9]+' "${accList}" 1>&2; then
  printf "ERROR: unexpected accession name(s) above from %s\n" "${devTodayList}" 1>&2
  exit 255
fi

export accCount=$(grep -c . "${accList}")
if [ "${accCount}" -eq 0 ]; then
  printf "ERROR: no assemblies have /contrib/%s/ files in %s\n" "${contribName}" "${devTodayList}" 1>&2
  printf "  check the spelling, or the files may be newer than today's listing\n" 1>&2
  exit 255
fi
printf "### %d assemblies have /contrib/%s/\n" "${accCount}" "${contribName}" 1>&2

##############################################################################
### 2. the release lists in the otto kent tree
##############################################################################
function inList() {
  grep -s -q -x -F -- "${1}" "${2}"
}

git -C "${ottoKentTree}" pull --ff-only

if [ "${releaseTier}" = "beta" ]; then
  if ! inList "${contribName}" "${betaGenArkList}"; then
    printf "ERROR: '%s' is not in %s\n" "${contribName}" "${betaGenArkList}" 1>&2
    exit 255
  fi
elif ! inList "${contribName}" "${publicGenArkList}"; then
  if ! inList "${contribName}" "${betaGenArkList}"; then
    printf "ERROR: '%s' is in neither %s nor %s\n" "${contribName}" "${publicGenArkList}" "${betaGenArkList}" 1>&2
    exit 255
  fi
  printf "### adding '%s' to %s refs #%s\n" "${contribName}" "${publicGenArkList}" "${redmineNumber}" 1>&2
  # make sure the new name starts on its own line
  if [ -s "${publicGenArkList}" ] && [ -n "$(tail -c 1 "${publicGenArkList}")" ]; then
    printf "\n" >> "${publicGenArkList}"
  fi
  printf "%s\n" "${contribName}" >> "${publicGenArkList}"
  git -C "${ottoKentTree}" add -- "${listDir}/publicGenArk.txt"
  git -C "${ottoKentTree}" commit \
    -m "${contribName} GenArk contrib tracks released to public refs #${redmineNumber}" \
    -- "${listDir}/publicGenArk.txt"
  if ! git -C "${ottoKentTree}" push origin master; then
    printf "ERROR: git push of publicGenArk.txt failed in %s\n" "${ottoKentTree}" 1>&2
    printf "  ottoBuildGenArkHub.py would reset the tree and lose the commit, not continuing\n" 1>&2
    exit 255
  fi
fi

##############################################################################
### 3. rebuild trackDb.txt and the hub.txt files for these assemblies
##############################################################################
### ottoBuildGenArkHub.py shares its lock with the otto cron and exits
### at once when the cron is building.  Wait for that lock to be free,
### then try; if the cron took the lock again in between, wait and retry.
export buildLog=$(mktemp /dev/shm/pushContrib.build.XXXXXX)
trap 'rm -f "${accList}" "${buildLog}"' EXIT
export lockWait=7200
export maxTries=5
export tryCount=0
while true
do
  tryCount=$((tryCount + 1))
  if ! flock -w "${lockWait}" "${ottoBuildLock}" true; then
    printf "ERROR: otto build lock %s still held after %d seconds, nothing has been pushed\n" "${ottoBuildLock}" "${lockWait}" 1>&2
    exit 255
  fi
  printf "### %s -f %s %s\n" "${ottoBuild}" "${accList}" "$(date '+%F %T')" 1>&2
  set +e
  "${ottoBuild}" -f "${accList}" 2>&1 | tee "${buildLog}"
  buildStatus=${PIPESTATUS[0]}
  set -e
  if [ "${buildStatus}" -eq 0 ]; then
    break
  fi
  if grep -q "another otto build is already running" "${buildLog}" \
       && [ "${tryCount}" -lt "${maxTries}" ]; then
    printf "# otto cron took the build lock first, waiting again (try %d of %d)\n" "${tryCount}" "${maxTries}" 1>&2
    sleep 60
    continue
  fi
  printf "ERROR: ottoBuildGenArkHub.py failed, nothing has been pushed\n" 1>&2
  exit 255
done

##############################################################################
### 4. push them out
##############################################################################
export rrOpt=""
if [ "${releaseTier}" = "public" ]; then
  rrOpt="-rr"
fi
mapfile -t accArray < "${accList}"
printf "### %s %s %s %d assemblies %s\n" "${pushAccessions}" "${rrOpt}" "${fullPushOpt}" "${accCount}" "$(date '+%F %T')" 1>&2
"${pushAccessions}" ${rrOpt} ${fullPushOpt} "${accArray[@]}"
