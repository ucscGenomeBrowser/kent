#!/bin/bash

# ottoRequestWatch.sh - drive the alignment pipeline from ottoRequest table
#
# Intended to run from cron under the user's own account (not the
# web-server service user).  Picks up requests that ottoRequest.py has
# acknowledged (status=1) and drives them through alignment setup
# and workflow monitoring.
#
# Phase 1: new requests needing alignment setup - status=1 AND buildDir=''
#          run ottoRequestAlign.sh to set up and launch the workflow
# Phase 2: in-progress requests needing workflow monitoring
#          run workflowMonitor.sh to poll Galaxy and install results
#   0 pending, 1 notified, 2 in progress, 3 galaxy done, 4 tracks complete,
#   5 ready to push, 6 push is done, 7 problems,
#      8 final notification has been sent == process is complete
### cron job entry:
#9,20,31,42,53 * * * * ~/kent/src/hg/utils/otto/userRequests/ottoRequestWatch.sh

set -eEu -o pipefail
umask 002

export scriptDir=$(cd "$(dirname "$0")" && pwd)

##############################################################################
### singleton lock - only one instance at a time
### Open lockPath on FD 9 for the lifetime of the shell, then take a
### non-blocking exclusive lock.  Kernel releases the lock on exit
### (normal, error, or kill -9), so no stale lock cleanup is needed.
### Exit 0 silently if another instance holds the lock so cron doesn't
### email on every overlapping tick.  PID is written to the file for
### information only see the holder via:
###   cat ottoRequestWatch.lock      (the PID)
###   lsof ottoRequestWatch.lock     (the locking process)
##############################################################################
export lockPath="${scriptDir}/ottoRequestWatch.lock"
# 9<> opens read+write without truncating, so a second instance that
# comes along while we're running won't wipe our PID from the file
# before its flock attempt fails.
exec 9<>"${lockPath}"
flock -n 9 || exit 0
# we own the lock now safe to truncate and write our PID.  ': >file'
# truncates via a separate FD; FD 9 keeps its position 0 from <>, so
# the printf below starts writing at the beginning of the empty file.
: >"${lockPath}"
printf "%d\n" "$$" >&9
##############################################################################

##############################################################################
### errors - set error status in the table
function setErrorStatus() {
  id="${1}"
  /cluster/bin/x86_64/hgsql -N -e \
      "UPDATE ottoRequest SET status=7 WHERE id=${id};" hgcentraltest
}
##############################################################################

##############################################################################
### getFeatureBitsPct - get percentage coverage from featureBits file
###   args: srcDb dstDb buildDir
###   returns percentage string (e.g., "45.2%") or empty string if not found
###   mimics the featureBitsPct() function from ottoRequestView.cgi
##############################################################################
function getFeatureBitsPct() {
  local srcDb="${1}"
  local dstDb="${2}"
  local buildDir="${3}"
  local DstDb="${dstDb^}"  # first letter capitalized

  if [[ -z "${srcDb}" || -z "${dstDb}" || -z "${buildDir}" ]]; then
    return
  fi

  # determine subdirectory: trackData for GenArk, bed for UCSC native
  local sub
  if [[ "${srcDb}" == GC* ]]; then
    sub="trackData"
  else
    sub="bed"
  fi

  # construct path to featureBits file
#  local fbFile="${buildDir}/${sub}/lastz.${dstDb}/fb.${srcDb}.chain${DstDb}Link.txt"
  local fbFile="${buildDir}/fb.${srcDb}.chain${DstDb}Link.txt"

  if [[ -f "${fbFile}" ]]; then
    # extract percentage from file using grep + sed (matches Python regex r'\(([\d.]+)%\)')
    local pct
    pct="$(grep -oE '\([0-9.]+%\)' "${fbFile}" 2>/dev/null | sed 's/[()]//g' | head -1)"
    if [[ -n "${pct}" ]]; then
      printf "%s" "${pct}"
    fi
  fi
}
##############################################################################

##############################################################################
### liftOverUrl - build the public download URL for an over.chain.gz file
###   args: srcDb dstDb
###   GenArk:      https://hgdownload.soe.ucsc.edu/hubs/<3>/<3>/<3>/<3>/<acc>/liftOver/<srcDb>To<DstDb>.over.chain.gz
###   UCSC native: https://hgdownload.soe.ucsc.edu/goldenPath/<srcDb>/liftOver/<srcDb>To<DstDb>.over.chain.gz
###   DstDb is dstDb with the first letter upper-cased (matches the
###   filename convention used in installLinks()).
###   verifies the URL with a curl HEAD before printing; returns 1 if
###   the URL does not resolve to a 2xx response.
##############################################################################
function liftOverUrl() {
  local srcDb="${1}"
  local dstDb="${2}"
  local DstDb="${dstDb^}"
  local fileName="${srcDb}To${DstDb}.over.chain.gz"
  local url
  if [[ "${srcDb}" == GC* ]]; then
    local gcX="${srcDb:0:3}"
    local d0="${srcDb:4:3}"
    local d1="${srcDb:7:3}"
    local d2="${srcDb:10:3}"
    local accPath="${gcX}/${d0}/${d1}/${d2}/${srcDb}"
    url="https://hgdownload.soe.ucsc.edu/hubs/${accPath}/liftOver/${fileName}"
  else
    url="https://hgdownload.soe.ucsc.edu/goldenPath/${srcDb}/liftOver/${fileName}"
  fi
  # -s silent, -f fail on HTTP >= 400, -I HEAD, --max-time bounds hangs
  if ! curl -sfI --max-time 30 -o /dev/null "${url}"; then
    printf "ERROR: liftOverUrl: URL does not exist: %s\n" "${url}" 1>&2
    return 1
  fi
  printf "%s" "${url}"
}
##############################################################################

##############################################################################
### sendNotification - email the requesting user that their alignment is done
###   args: reqId subject
###   message body is read from stdin
###   recipient: email column of ottoRequest table for that reqId
###   bcc: chain-file-request-group@ucsc.edu
###   envelope sender / Return-Path / bounce: genome-www@soe.ucsc.edu
###   returns 0 on success, non-zero on failure
##############################################################################
function sendNotification() {
  local reqId="${1}"
  local subject="${2}"
  local msgBody="${3}"
  local toAddr
  toAddr="$(/cluster/bin/x86_64/hgsql -N -B -e \
    "SELECT email FROM ottoRequest WHERE id = ${reqId};" hgcentraltest)"
  if [ -z "${toAddr}" ]; then
    printf "ERROR: sendNotification: no email for request %s\n" "${reqId}" 1>&2
    return 1
  fi
  local bcc="chain-file-request-group@ucsc.edu"
  local from="genome-www@soe.ucsc.edu"
  local bounce="gb"
  bounce+="aut"
  bounce+="o"
  bounce+="@"
  bounce+="uc"
  bounce+="sc."
  bounce+="ed"
  bounce+="u"
  # -f sets the envelope sender (becomes Return-Path at delivery and the
  #    bounce address); -t reads recipients from To:/Cc:/Bcc: headers;
  #    -oi prevents a lone "." in body from ending the message
  {
    printf "From: %s\n" "${from}"
    printf "To: %s\n" "${toAddr}"
    printf "Bcc: %s\n" "${bcc}"
    printf "Reply-To: %s\n" "${from}"
    printf "Subject: %s\n" "${subject}"
    printf "\n"
    printf "%s\n" "${msgBody}"
  } | /usr/sbin/sendmail -f "${bounce}" -t -oi
}
##############################################################################

##############################################################################
### installLinks - drop chain/quickLift symlinks and register in hgcentraltest
###   args: tDb qDb buildDir
###   detects GenArk (tDb starts with "GC") vs UCSC db and chooses the
###   matching branch from doBlastzChainNet.pl loadUp().
###   returns 0 on success, non-zero on failure.
##############################################################################
function installLinks() {
  local tDb="${1}"
  local qDb="${2}"
  local localBuildDir="${3}"
  local QDb="${qDb^}"
  local over="${tDb}To${QDb}.over.chain.gz"
  local quick="${qDb}"
  local axtDir="${localBuildDir}/axtChain"
  local overChain="${axtDir}/${tDb}.${qDb}.over.chain.gz"
  local quickBb="${axtDir}/${tDb}.${qDb}.quick.bb"
  local quickLinkBb="${axtDir}/${tDb}.${qDb}.quickLink.bb"

  if [ ! -s "${overChain}" ]; then
    printf "ERROR: installLinks: missing %s\n" "${overChain}" 1>&2
    return 1
  fi
  if [ ! -s "${quickBb}" ] || [ ! -s "${quickLinkBb}" ]; then
    printf "ERROR: installLinks: missing %s or %s\n" \
      "${quickBb}" "${quickLinkBb}" 1>&2
    return 1
  fi

  local liftOverDir
  local chainPath  # absolute path to .over.chain.gz that gets registered
  local quickPath  # absolute path to .quick.bb that gets registered

  if [[ "${tDb}" == GC* ]]; then
    # GenArk: full asmId is two dirs above buildDir
    #   .../<asmId>/trackData/lastz<Q>.YYYY-MM-DD
    local tAsmId
    tAsmId="$(basename "$(dirname "$(dirname "${localBuildDir}")")")"
    local hubBuild="/hive/data/genomes/asmHubs/allBuild/${tAsmId:0:3}/${tAsmId:4:3}/${tAsmId:7:3}/${tAsmId:10:3}/${tAsmId}"
    liftOverDir="${hubBuild}/liftOver"
    local genArkQuickDir="${hubBuild}/quickLift"
    local accession
    accession="$(echo "${tAsmId}" | cut -d'_' -f1-2)"
    local accessionPath="${tAsmId:0:3}/${tAsmId:4:3}/${tAsmId:7:3}/${tAsmId:10:3}/${accession}"
    local genArkQuickLinkDir="/hive/data/genomes/asmHubs/${accessionPath}/quickLift"
    local genArkOverLinkDir="/hive/data/genomes/asmHubs/${accessionPath}/liftOver"
    local genArkGbdbQuickDir="/gbdb/genark/${accessionPath}/quickLift"
    local genArkGbdbOverDir="/gbdb/genark/${accessionPath}/liftOver"
    # ../trackData/... relative form so the link is portable across mirrors
    local genArkTrackLink="${axtDir}/${tDb}.${qDb}.quick"
    genArkTrackLink="../trackData${genArkTrackLink##*/trackData}"

    # place the .over.chain.gz where genArk infrastructure expects it
    mkdir -p "${liftOverDir}"
    rm -f "${liftOverDir}/${over}"
    ln -s "${overChain}" "${liftOverDir}/${over}"

    # quickLift bigBeds and the staging/gbdb chain of symlinks
    mkdir -p "${genArkQuickDir}"
    rm -f "${genArkQuickDir}/${quick}.bb"
    rm -f "${genArkQuickDir}/${quick}.link.bb"
    rm -f "${genArkQuickLinkDir}" "${genArkGbdbQuickDir}"
    rm -f "${genArkOverLinkDir}" "${genArkGbdbOverDir}"
    ln -s "${genArkTrackLink}.bb" "${genArkQuickDir}/${quick}.bb"
    ln -s "${genArkTrackLink}Link.bb" "${genArkQuickDir}/${quick}.link.bb"
    ln -s "${genArkQuickDir}" "${genArkQuickLinkDir}"
    ln -s "${genArkQuickLinkDir}" "${genArkGbdbQuickDir}"
    ln -s "${liftOverDir}" "${genArkOverLinkDir}"
    ln -s "${genArkOverLinkDir}" "${genArkGbdbOverDir}"

    chainPath="${genArkGbdbOverDir}/${over}"
    quickPath="${genArkGbdbQuickDir}/${quick}.bb"
  else
    # UCSC native db
    liftOverDir="/hive/data/genomes/${tDb}/bed/liftOver"
    local gbdbLiftOverDir="/gbdb/${tDb}/liftOver"
    local gbdbQuickLiftDir="/gbdb/${tDb}/quickLift"

    mkdir -p "${liftOverDir}"
    rm -f "${liftOverDir}/${over}"
    ln -s "${overChain}" "${liftOverDir}/${over}"

    mkdir -p "${gbdbLiftOverDir}" "${gbdbQuickLiftDir}"
    rm -f "${gbdbLiftOverDir}/${over}"
    rm -f "${gbdbQuickLiftDir}/${quick}.bb"
    rm -f "${gbdbQuickLiftDir}/${quick}.link.bb"
    ln -s "${quickBb}" "${gbdbQuickLiftDir}/${quick}.bb"
    ln -s "${quickLinkBb}" "${gbdbQuickLiftDir}/${quick}.link.bb"
    ln -s "${liftOverDir}/${over}" "${gbdbLiftOverDir}/${over}"

    chainPath="${gbdbLiftOverDir}/${over}"
    quickPath="${gbdbQuickLiftDir}/${quick}.bb"
  fi

  # register both rows in hgcentraltest
  if ! /cluster/bin/x86_64/hgAddLiftOverChain -minMatch=0.1 -multiple \
      -path="${chainPath}" "${tDb}" "${qDb}" > /dev/null 2>&1; then
    printf "ERROR: installLinks: hgAddLiftOverChain failed for %s -> %s\n" \
      "${tDb}" "${qDb}" 1>&2
    return 1
  fi
  if ! "${HOME}/kent/src/hg/utils/automation/addQuickLift.py" \
      "${tDb}" "${qDb}" "${quickPath}" > /dev/null 2>&1; then
    printf "ERROR: installLinks: addQuickLift.py failed for %s -> %s\n" \
      "${tDb}" "${qDb}" 1>&2
    return 1
  fi
  return 0
}
##############################################################################

##############################################################################
### refresh Galaxy queue status snapshot for ottoRequestView.cgi
##############################################################################
export galaxyStatusFile="/data/apache/trash/ottoRequestGalaxyStatus.json"
export profileJson="${HOME}/.planemo/profiles/vgp/planemo_profile_options.json"
gsTmp=$(mktemp "${galaxyStatusFile}.XXXXXX")
if timeout 45 "${scriptDir}/galaxyStatus.py" "${profileJson}" > "${gsTmp}" 2>/dev/null; then
  chmod 664 "${gsTmp}"
  mv "${gsTmp}" "${galaxyStatusFile}"
else
  rm -f "${gsTmp}"
#  printf "# WARNING: galaxyStatus.py failed, leaving stale snapshot\n" 1>&2
fi
##############################################################################

##############################################################################
### refresh featureBits coverage snapshot for ottoRequestView.cgi
###   append-only; script reads its current snapshot and only measures
###   pairs not already recorded, so per-tick cost is ~zero in steady
###   state.  Has its own singleton lock, writes its own snapshot file
###   atomically, exits 0 silently when nothing to do.
##############################################################################
if ! timeout 45 "${scriptDir}/featureBitsSnapshot.py" 2>/dev/null; then
  : # non-zero exit ignored: leave stale snapshot, next tick will retry
fi
##############################################################################

############################################################################
# phase 0: pre-flight existing-work detection.  If the alignment has
#          already been built in-house (legacy lastz/chain/net or an
#          earlier kegAlign run), the only step left is the hgdownload
#          push.  Signals (all must hold):
#            /hive/data/genomes/${fromDb}/bed/lastz.${toDb}  symlink exists
#            /hive/data/genomes/${toDb}/bed/lastz.${fromDb}  symlink exists
#            hgcentraltest.liftOverChain   has both directions
#            hgcentraltest.quickLiftChain  has both directions
#          When all four hold, fill in buildDir with the resolved
#          fromDb-side build dir and bump status=5 so
#          ottoRequestPush.py picks it up.  Anything that doesn't match
#          stays at status=1 and falls through to phase 1.
############################################################################
while IFS=$'\t' read -r reqId fromDb toDb; do
  fromSym="/hive/data/genomes/${fromDb}/bed/lastz.${toDb}"
  toSym="/hive/data/genomes/${toDb}/bed/lastz.${fromDb}"
  if [ ! -L "${fromSym}" ] || [ ! -L "${toSym}" ]; then
    continue
  fi
  fromBuild="$(readlink -f "${fromSym}")"
  toBuild="$(readlink -f "${toSym}")"
  if [ ! -d "${fromBuild}" ] || [ ! -d "${toBuild}" ]; then
    continue
  fi
  loCount=$(/cluster/bin/x86_64/hgsql -N -B -e \
    "SELECT COUNT(*) FROM liftOverChain WHERE \
       (fromDb='${fromDb}' AND toDb='${toDb}') OR \
       (fromDb='${toDb}'   AND toDb='${fromDb}');" hgcentraltest)
  qlCount=$(/cluster/bin/x86_64/hgsql -N -B -e \
    "SELECT COUNT(*) FROM quickLiftChain WHERE \
       (fromDb='${fromDb}' AND toDb='${toDb}') OR \
       (fromDb='${toDb}'   AND toDb='${fromDb}');" hgcentraltest)
  if [ "${loCount}" -lt 2 ] || [ "${qlCount}" -lt 2 ]; then
    continue
  fi
  printf "# request %s: prior work detected at %s, jumping to push\n" \
    "${reqId}" "${fromBuild}" 1>&2
  /cluster/bin/x86_64/hgsql -N -e \
    "UPDATE ottoRequest SET status=5, buildDir='${fromBuild}' \
     WHERE id=${reqId};" hgcentraltest
done < <(/cluster/bin/x86_64/hgsql -N -B -e \
  "SELECT id, fromDb, toDb FROM ottoRequest \
   WHERE status = 1 AND buildDir = '' AND requestType = 'liftOver';" hgcentraltest)

############################################################################
# phase 1: new requests needing alignment setup - status=1 AND buildDir=''
############################################################################
while read -r reqId; do
# printf "# starting alignment for request %s\n" "${reqId}" 1>&2
  if ! "${scriptDir}/ottoRequestAlign.sh" "${reqId}"; then
    printf "# alignment setup FAILED for request %s\n" "${reqId}" 1>&2
    setErrorStatus "${reqId}"
  fi
done < <(/cluster/bin/x86_64/hgsql -N -B -e \
  "SELECT id FROM ottoRequest WHERE status = 1 AND buildDir = '' AND requestType = 'liftOver';" \
  hgcentraltest)

############################################################################
# phase 2: in-progress requests needing workflow monitoring
############################################################################
while IFS=$'\t' read -r reqId buildDir; do
  if [ ! -d "${buildDir}" ]; then
    printf "# WARNING: buildDir not found for request %s: %s\n" \
      "${reqId}" "${buildDir}" 1>&2
    continue
  fi
  # takes a while for the galaxy WF to start up, wait for this file to appear
  if [ ! -s "${buildDir}/pendingInvocationId.txt" ]; then
    continue
  fi
# printf "# monitoring request %s: %s\n" "${reqId}" "${buildDir}" 1>&2
  if "${scriptDir}/workflowMonitor.sh" "${reqId}" "${buildDir}"; then
    # workflowMonitor.sh exits 0 both when still running and when complete;
    # check for the success marker to distinguish
    if [ -s "${buildDir}/successInvocationId.txt" ]; then
      /cluster/bin/x86_64/hgsql -N -e \
        "UPDATE ottoRequest SET status = 4, completeTime = NOW() \
         WHERE id = ${reqId};" hgcentraltest
#     printf "# request %s completed successfully\n" "${reqId}" 1>&2
    fi
    # else: still running, will check again next invocation
  else
    printf "# workflow error for request %s\n" "${reqId}" 1>&2
    setErrorStatus "${reqId}"
  fi
done < <(/cluster/bin/x86_64/hgsql -N -B -e \
  "SELECT id, buildDir FROM ottoRequest \
   WHERE status = 2 AND buildDir != '' AND requestType = 'liftOver';" hgcentraltest)

############################################################################
# phase 3: check for tracks done, setup symlinks set status=5 to indicate
#          ready to push
############################################################################
while IFS=$'\t' read -r reqId buildDir; do
  if [ ! -d "${buildDir}" ]; then
    printf "# WARNING: buildDir not found for request %s: %s\n" \
      "${reqId}" "${buildDir}" 1>&2
    continue
  fi
  source <(grep -E '^export (swapDir|targetDb|queryDb)=' "${buildDir}/kegAlign.sh")
  export trackData="$(dirname "${buildDir}")"
  export swapData="$(dirname "${swapDir}")"
  export workDir="$(basename "${buildDir}")"
  export swapWork="$(basename "${swapDir}")"

  # target direction: targetDb is target, queryDb is query, work in buildDir.
  # GenArk targets use doTrackDb.bash sitting next to the trackData dir;
  # UCSC native targets use chainNetTrackDb.pl which probes axtChain/*.bb
  # in the build dir and writes a stanza into the otto kent clone.
  rm -f "${trackData}/lastz.${queryDb}"
  ln -s "${workDir}" "${trackData}/lastz.${queryDb}"
  case "${targetDb}" in
    GC[AF]_*)
      doTdb="$(dirname "${trackData}")/doTrackDb.bash"
      if [ ! -x "${doTdb}" ]; then
        printf "ERROR: can not find %s\n" "${doTdb}" 1>&2
        setErrorStatus "${reqId}"
        continue
      fi
      if ! "${doTdb}" >> /dev/shm/doTdb.$$.log 2>&1; then
        printf "ERROR: %s failed\n" "${doTdb}" 1>&2
        cat /dev/shm/doTdb.$$.log 1>&2
        rm -f /dev/shm/doTdb.$$.log
        setErrorStatus "${reqId}"
        continue
      fi
      rm -f /dev/shm/doTdb.$$.log
      ;;
    *)
      if ! ( cd "${buildDir}" \
             && "${scriptDir}/chainNetTrackDb.pl" \
                  "${targetDb}" "${queryDb}" ); then
        printf "ERROR: chainNetTrackDb.pl failed for %s/%s\n" \
          "${targetDb}" "${queryDb}" 1>&2
        setErrorStatus "${reqId}"
        continue
      fi
      ;;
  esac

  # swap direction: queryDb is target, targetDb is query, work in swapDir
  rm -f "${swapData}/lastz.${targetDb}"
  ln -s "${swapWork}" "${swapData}/lastz.${targetDb}"
  case "${queryDb}" in
    GC[AF]_*)
      swapTdb="$(dirname "${swapData}")/doTrackDb.bash"
      if [ ! -x "${swapTdb}" ]; then
        printf "ERROR: can not find %s\n" "${swapTdb}" 1>&2
        setErrorStatus "${reqId}"
        continue
      fi
      if ! "${swapTdb}" >> /dev/shm/swapTdb.$$.log 2>&1; then
        printf "ERROR: %s failed\n" "${swapTdb}" 1>&2
        cat /dev/shm/swapTdb.$$.log 1>&2
        rm -f /dev/shm/swapTdb.$$.log
        setErrorStatus "${reqId}"
        continue
      fi
      rm -f /dev/shm/swapTdb.$$.log
      ;;
    *)
      if ! ( cd "${swapDir}" \
             && "${scriptDir}/chainNetTrackDb.pl" \
                  "${queryDb}" "${targetDb}" ); then
        printf "ERROR: chainNetTrackDb.pl failed for %s/%s\n" \
          "${queryDb}" "${targetDb}" 1>&2
        setErrorStatus "${reqId}"
        continue
      fi
      ;;
  esac

  # install liftOver and quickLift symlinks + register in hgcentraltest,
  # for both directions
  if ! installLinks "${targetDb}" "${queryDb}" "${buildDir}"; then
    setErrorStatus "${reqId}"
    continue
  fi
  if ! installLinks "${queryDb}" "${targetDb}" "${swapDir}"; then
    setErrorStatus "${reqId}"
    continue
  fi

  /cluster/bin/x86_64/hgsql -N -e \
      "UPDATE ottoRequest SET status = 5 WHERE id=${reqId};" hgcentraltest
done < <(/cluster/bin/x86_64/hgsql -N -B -e \
  "SELECT id, buildDir FROM ottoRequest \
   WHERE status = 4 AND buildDir != '' AND requestType = 'liftOver';" hgcentraltest)

############################################################################
# phase 4: check for push files is complete, send final notification
#          clean up galaxy workflow
############################################################################

while IFS=$'\t' read -r reqId fromDb toDb comment requestTime buildDir; do
  # time to clean up the galaxy history and workflow to release the space
  if [ -s "${buildDir}/successInvocationId.txt" ]; then
    invocationId=$(cut -f2 "${buildDir}/successInvocationId.txt")
    if ! "${scriptDir}/galaxyCleanup.py" "${profileJson}" "${invocationId}"; then
      printf "# WARNING: galaxy cleanup failed for request %s\n" "${reqId}" 1>&2
    fi
  fi

  if ! fromUrl="$(liftOverUrl "${fromDb}" "${toDb}")"; then
    setErrorStatus "${reqId}"
    continue
  fi
  if ! toUrl="$(liftOverUrl "${toDb}" "${fromDb}")"; then
    setErrorStatus "${reqId}"
    continue
  fi
  # source the kegAlign.sh variables to get targetDb, queryDb, and swapDir
  targetDb="" queryDb="" swapDir=""
  if [[ -f "${buildDir}/kegAlign.sh" ]]; then
    source <(grep -E '^export (swapDir|targetDb|queryDb)=' "${buildDir}/kegAlign.sh" 2>/dev/null || true)
  fi
  # get featureBits coverage percentages for both directions
  # determine which direction is which and use appropriate build directory
  fromToPct="" toFromPct=""
  if [[ -n "${targetDb}" && -n "${queryDb}" && -n "${swapDir}" ]]; then
    if [[ "${fromDb}" == "${targetDb}" ]]; then
      # fromDb -> toDb uses buildDir, toDb -> fromDb uses swapDir
      fromToPct="$(getFeatureBitsPct "${fromDb}" "${toDb}" "${buildDir}")"
      toFromPct="$(getFeatureBitsPct "${toDb}" "${fromDb}" "${swapDir}")"
    else
      # fromDb -> toDb uses swapDir, toDb -> fromDb uses buildDir
      fromToPct="$(getFeatureBitsPct "${fromDb}" "${toDb}" "${swapDir}")"
      toFromPct="$(getFeatureBitsPct "${toDb}" "${fromDb}" "${buildDir}")"
    fi
  else
    # fallback: try both directions with buildDir only (may not find swap direction)
    fromToPct="$(getFeatureBitsPct "${fromDb}" "${toDb}" "${buildDir}")"
    toFromPct="$(getFeatureBitsPct "${toDb}" "${fromDb}" "${buildDir}")"
  fi

  # construct coverage info for email
  coverageInfo=""
  if [[ -n "${fromToPct}" || -n "${toFromPct}" ]]; then
    coverageInfo="
Chain coverage (% of genome covered by alignments):
  ${fromDb} -> ${toDb}: ${fromToPct:-"not available"}
  ${toDb} -> ${fromDb}: ${toFromPct:-"not available"}
"
  fi

  sendNotification "${reqId}" \
"from UCSC: liftOverRequest complete: ${fromDb}<->${toDb}" \
"Your lift over request is complete:
     From:     ${fromDb}
       To:     ${toDb}
  comment:     ${comment}
submitted:     ${requestTime}
${coverageInfo}
The lift.over files are available at these links:

  ${fromUrl}
  ${toUrl}
"

  /cluster/bin/x86_64/hgsql -N -e \
      "UPDATE ottoRequest SET status=8, completeTime=now() WHERE id=${reqId};" hgcentraltest

done < <(/cluster/bin/x86_64/hgsql -N -B -e \
  "SELECT id, fromDb, toDb, comment, requestTime, buildDir FROM ottoRequest \
   WHERE status = 6 AND requestType = 'liftOver';" hgcentraltest)

