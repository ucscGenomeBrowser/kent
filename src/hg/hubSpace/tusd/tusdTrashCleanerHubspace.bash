#!/bin/bash

set -beEu -o pipefail

# the tusd data directory, different on each machine
TUSD_DATA_DIR="/data/tusd/data"
TODAY=$(date +%F)
ARCHIVE_DIR="/tmp/tusdTrash"
DELETE_FILE_NAME="toDelete.txt"
# a one line per run history, quiet runs send no email so this is the
# only proof the cron actually fired
LOG_DIR="/home/qateam/hubSpace"
LOG_FILE="${LOG_DIR}/tusdTrashCleaner.log"
# exit code telling the wrapper there was nothing to delete, so no email
NOTHING_TO_DO=100

export hostname=""
hostname=$(hostname -s)
if [ "${hostname}" != "hubspace" ]; then
    printf "stopping because not on hubspace machine\n"
    exit
fi

# make the email easier to read by stating where we are
printf "running tusd trash cleaner on %s\n" "${hostname}"

# ensure the archive_dir exists, the dated subdir is only made when we
# actually delete something, so we don't leave empty dirs behind
mkdir -p "${ARCHIVE_DIR}"
if [ ! -d "${logDir}" ]; then
    /bin/mkdir -p "${logDir}"
    /bin/chmod 777 "${logDir}"
fi
touch "${LOG_FILE}"
/bin/chmod 666 "${LOG_FILE}"
cd "${ARCHIVE_DIR}"

# get all the files to remove that are at least 2 days old
find "${TUSD_DATA_DIR}" -maxdepth 1 -mtime +1 -type f  | sort > "${DELETE_FILE_NAME}"

delCount=$(wc -l < "${DELETE_FILE_NAME}")
if [ "${delCount}" -eq 0 ];
then
    # nothing to do, don't leave the empty list behind
    rm -f "${DELETE_FILE_NAME}"
    printf "%s ran, nothing to delete\n" "$(date '+%F %T')" >> "${LOG_FILE}"
    printf "TUSD trash cleaner: no files to delete today\n"
    exit "${NOTHING_TO_DO}"
else
    # delete those files
    cat "${DELETE_FILE_NAME}" | /bin/parallel rm -f 

    # move the file list to a record keeping location
    mkdir -p "${ARCHIVE_DIR}/${TODAY}"
    mv "${DELETE_FILE_NAME}" "${ARCHIVE_DIR}/${TODAY}/${DELETE_FILE_NAME}"

    printf "%s deleted %s files\n" "$(date '+%F %T')" "${delCount}" >> "${LOG_FILE}"

    # send out a completion message
    printf "TUSD trash cleaner finished on %s, cleaned: %s\n" "${TODAY}" "${TUSD_DATA_DIR}"
fi
