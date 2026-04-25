#!/bin/bash

# ottoRequestWatch.sh - drive the alignment pipeline from ottoRequest table
#
# Intended to run from cron under the user's own account (not the
# web-server service user).  Picks up requests that ottoRequest.py has
# acknowledged (status=1) and drives them through alignment setup
# and workflow monitoring.
#
# Phase 1: status=1 with empty buildDir
#          run ottoRequestAlign.sh to set up and launch the workflow
# Phase 2: status=2 with buildDir set
#          run workflowMonitor.sh to poll Galaxy and install results
#   0 pending, 1 notified, 2 in progress, 3 galaxy done, 4 tracks complete,
#   5 finish notification, 6 complete, 7 problems */

set -beEu -o pipefail

export scriptDir=$(cd "$(dirname "$0")" && pwd)

##############################################################################
### errors - set error status in the table
function setErrorStatus() {
  id="${1}"
  hgsql -N -e \
      "UPDATE ottoRequest SET status=7 WHERE id=${id};" hgcentraltest
}
##############################################################################

############################################################################
# phase 1: new requests needing alignment setup - status=1 AND buildDir=''
############################################################################
while read -r reqId; do
  printf "# starting alignment for request %s\n" "${reqId}" 1>&2
  if "${scriptDir}/ottoRequestAlign.sh" "${reqId}"; then
    printf "# alignment setup complete for request %s\n" "${reqId}" 1>&2
  else
    printf "# alignment setup FAILED for request %s\n" "${reqId}" 1>&2
    setErrorStatus $reqId
  fi
done < <(hgsql -N -B -e \
  "SELECT id FROM ottoRequest WHERE status = 1 AND buildDir = '';" \
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
  printf "# monitoring request %s: %s\n" "${reqId}" "${buildDir}" 1>&2
  if "${scriptDir}/workflowMonitor.sh" "${reqId}" "${buildDir}"; then
    # workflowMonitor.sh exits 0 both when still running and when complete;
    # check for the success marker to distinguish
    if [ -s "${buildDir}/successInvocationId.txt" ]; then
      hgsql -N -e \
        "UPDATE ottoRequest SET status = 2, completeTime = NOW() \
         WHERE id = ${reqId};" hgcentraltest
      printf "# request %s completed successfully\n" "${reqId}" 1>&2
    fi
    # else: still running, will check again next invocation
  else
    printf "# workflow error for request %s\n" "${reqId}" 1>&2
    setErrorStatus $reqId
  fi
done < <(hgsql -N -B -e \
  "SELECT id, buildDir FROM ottoRequest \
   WHERE status = 2 AND buildDir != '';" hgcentraltest)
