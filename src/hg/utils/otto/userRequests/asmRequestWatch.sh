#!/bin/bash

# asmRequestWatch.sh - what for assembly requests in the ottoRequest table
#
# Initial function is just to watch the status, and when it
#   reaches status 6 'push complete' (which currently is set manually)
#   then send email and mark finished
#
# The 'otto' user cron job 'ottoRequest.py' will be watching for status 0
#   for a new entry.  It will set status 1 and send notification email
#
# Different meanings from the liftOver status settings:
#   0 pending, 1 notified, 2 in progress, 6 push is done and is available on the RR,
#      status 7 for problems, and 8 is final notification has been sent == process is complete
### cron job entry: (1 minute later than ottoRequestWatch.sh)
# 10,21,32,43,54 * * * * ~/kent/src/hg/utils/otto/userRequests/asmRequestWatch.sh

set -eEu -o pipefail
umask 002

export scriptDir=$(cd "$(dirname "$0")" && pwd)
export centDb="hgcentral"
export hgSql="hgsql -hgenome-centdb"

##############################################################################
### singleton lock - only one instance at a time
### Open lockPath on FD 9 for the lifetime of the shell, then take a
### non-blocking exclusive lock.  Kernel releases the lock on exit
### (normal, error, or kill -9), so no stale lock cleanup is needed.
### Exit 0 silently if another instance holds the lock so cron doesn't
### email on every overlapping tick.  PID is written to the file for
### information only see the holder via:
###   cat asmRequestWatch.lock      (the PID)
###   lsof asmRequestWatch.lock     (the locking process)
##############################################################################
export lockPath="${scriptDir}/asmRequestWatch.lock"
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
  /cluster/bin/x86_64/${hgSql} -N -e \
      "UPDATE ottoRequest SET status=7 WHERE id=${id};" "${centDb}"
}
##############################################################################

##############################################################################
### sendNotification - email the requesting user that their assembly request is done
###   args: reqId subject
###   message body is read from stdin
###   recipient: email column of ottoRequest table for that reqId
###   bcc: genark-request-group@ucsc.edu
###   envelope sender / Return-Path / bounce: genome-www@soe.ucsc.edu
###   returns 0 on success, non-zero on failure
##############################################################################
function sendNotification() {
  local reqId="${1}"
  local subject="${2}"
  local msgBody="${3}"
  local toAddr
  toAddr="$(/cluster/bin/x86_64/${hgSql} -N -B -e \
    "SELECT email FROM ottoRequest WHERE id = ${reqId};" ${centDb})"
  if [ -z "${toAddr}" ]; then
    printf "ERROR: sendNotification: no email for request %s\n" "${reqId}" 1>&2
    return 1
  fi
  local bcc="genark-request-group@ucsc.edu"
  local from="genome-www@soe.ucsc.edu"
  local bounce="gb"
  bounce+="aut"
  bounce+="o"
  bounce+="@"
  bounce+="uc"
  bounce+="sc."
  bounce+="ed"
  bounce+="u"
  printf "# DBG sending\n" 1>&2
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
}	# function sendNotification()

############################################################################
# check for phase 6: the assembly is complete and available on the RR
#       this checking and setting status 6 is currently done manually,
#       eventually this will become automatic.
############################################################################
while IFS=$'\t' read -r reqId fromDb comment requestTime; do

  export gcX="${fromDb:0:3}"
  export d0="${fromDb:4:3}"
  export d1="${fromDb:7:3}"
  export d2="${fromDb:10:3}"
  export gbDbPath="/gbdb/genark/${gcX}/${d0}/${d1}/${d2}/${fromDb}/hub.txt"
  export hubTxt="https://genome.ucsc.edu/cgi-bin/hgTracks?genome=${fromDb}&hubUrl=${gbDbPath}"

  sendNotification "${reqId}" \
"from UCSC: assembly request complete: ${fromDb}" \
"from UCSC: Your assembly request is complete:
 assembly:     ${fromDb}
  comment:     ${comment}
submitted:     ${requestTime}

The assembly is available in the browser at the following URL:
  ${hubTxt}
"

/cluster/bin/x86_64/${hgSql} -N -e \
      "UPDATE ottoRequest SET status=8, completeTime=now() WHERE id=${reqId};" ${centDb}

done < <(/cluster/bin/x86_64/${hgSql} -N -B -e \
  "SELECT id, fromDb, comment, requestTime FROM ottoRequest \
   WHERE status = 6 AND requestType = 'assembly';" ${centDb})
