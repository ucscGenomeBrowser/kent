#!/bin/bash

# weekly cron wrapper for measureHubSpace.dev.sh, which writes its own dated
# report under /hive/users/qateam/hubSpace/hgwdev/YYYY/MM/.  Then the hubspace
# machine's reports are copied to /hive/users/qateam/hubSpace/hubspace/, and
# hubSpaceReport.py draws one charts page for each machine into
# /hive/users/qateam/hubSpace/.
#
# crontab entry, Mondays at 07:00, after the hubspace machine measures at 06:00:
# 0 7 * * 1 /cluster/home/chmalee/bin/measureHubSpaceWrapperHgwdev.bash

set -beEu -o pipefail

SCRIPTDIR="/cluster/home/chmalee/hubSpace"
MEASURE="${SCRIPTDIR}/measureHubSpace.dev.sh"
REPORT="${SCRIPTDIR}/hubSpaceReport.py"
DATAROOT="/hive/users/qateam/hubSpace"
DEVROOT="${DATAROOT}/hgwdev"
PRODROOT="${DATAROOT}/hubspace"

"${MEASURE}"

# only the dated YYYY directories, not the scripts or pages next to them
mkdir -p "${PRODROOT}"
rsync -a "qateam@hubspace:/home/qateam/hubSpace/[0-9][0-9][0-9][0-9]" "${PRODROOT}/"

# the pages load these from their own directory
cp "${SCRIPTDIR}/hubSpaceReport.css" "${SCRIPTDIR}/hubSpaceReport.js" "${DATAROOT}/"
"${REPORT}" -l hgwdev "${DEVROOT}" "${DATAROOT}/hgwdev.html"
"${REPORT}" -l hubspace "${PRODROOT}" "${DATAROOT}/production.html"
