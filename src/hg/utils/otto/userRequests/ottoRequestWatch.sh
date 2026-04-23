#!/bin/bash

# ottoRequestWatch.sh - drive the alignment pipeline from ottoRequest table
#
# Intended to run from cron under the user's own account (not the
# web-server service user).  Picks up requests that ottoRequest.py has
# acknowledged (doneStatus=1) and drives them through alignment setup
# and workflow monitoring.
#
# Phase 1: doneStatus=1 with empty buildDir
#          → run ottoRequestAlign.sh to set up and launch the workflow
# Phase 2: doneStatus=1 with buildDir set
#          → run workflowMonitor.sh to poll Galaxy and install results
#          → on completion: doneStatus=2, completeTime set
#          → on failure:    doneStatus=3

set -beEu -o pipefail

export scriptDir=$(cd "$(dirname "$0")" && pwd)

############################################################################
# phase 1: new requests needing alignment setup
############################################################################
while read -r reqId; do
  printf "# starting alignment for request %s\n" "${reqId}" 1>&2
  if "${scriptDir}/ottoRequestAlign.sh" "${reqId}"; then
    printf "# alignment setup complete for request %s\n" "${reqId}" 1>&2
  else
    printf "# alignment setup FAILED for request %s\n" "${reqId}" 1>&2
    hgsql -N -e \
      "UPDATE ottoRequest SET doneStatus = 3 WHERE id = ${reqId};" \
      hgcentraltest
  fi
done < <(hgsql -N -B -e \
  "SELECT id FROM ottoRequest WHERE doneStatus = 1 AND buildDir = '';" \
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
  if "${scriptDir}/workflowMonitor.sh" "${buildDir}"; then
    # workflowMonitor.sh exits 0 both when still running and when complete;
    # check for the success marker to distinguish
    if [ -s "${buildDir}/successInvocationId.txt" ]; then
      hgsql -N -e \
        "UPDATE ottoRequest SET doneStatus = 2, completeTime = NOW() \
         WHERE id = ${reqId};" hgcentraltest
      printf "# request %s completed successfully\n" "${reqId}" 1>&2
    fi
    # else: still running, will check again next invocation
  else
    printf "# workflow error for request %s\n" "${reqId}" 1>&2
    hgsql -N -e \
      "UPDATE ottoRequest SET doneStatus = 3 WHERE id = ${reqId};" \
      hgcentraltest
  fi
done < <(hgsql -N -B -e \
  "SELECT id, buildDir FROM ottoRequest \
   WHERE doneStatus = 1 AND buildDir != '';" hgcentraltest)
