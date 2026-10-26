#!/bin/bash

set -beEu -o pipefail

TRASH_CLEANER="/cluster/home/chmalee/bin/tusdTrashCleanerHgwdev.bash"
VICTIM_LIST="chmalee@ucsc.edu"
# the cleaner uses this exit code when there was nothing to delete
NOTHING_TO_DO=100

# run the trash cleaner and hang on to its output, the || keeps set -e from
# killing us before we can look at the exit code
rc=0
output=$("${TRASH_CLEANER}" 2>&1) || rc=$?

# quiet run, nothing was deleted, so don't bother anybody
if [ "${rc}" -eq "${NOTHING_TO_DO}" ]; then
    exit 0
fi

# something happened, either real work or a failure, so send the mail
printf "%s\n" "${output}" | mailx -s "hgwdev TUSD trash cleaner" "${VICTIM_LIST}"

exit "${rc}"
