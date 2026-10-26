#!/bin/bash

# weekly cron wrapper for measureHubSpace.sh, which writes its own dated report
# under /home/qateam/hubSpace/YYYY/MM/, then hubSpaceReport.py redraws the
# charts page from all of the reports.  A cron job on hgwdev copies the page
# into ~chmalee/public_html/hubSpace/ next to the hgwdev one.
#
# crontab entry, Mondays at 06:00:
# 0 6 * * 1 /home/qateam/hubSpace/measureHubSpaceWrapperHubspace.bash

set -beEu -o pipefail

MEASURE="/home/qateam/hubSpace/measureHubSpace.sh"
REPORT="/home/qateam/hubSpace/hubSpaceReport.py"
LOGROOT="/home/qateam/hubSpace"

"${MEASURE}"

"${REPORT}" -l hubspace "${LOGROOT}" "${LOGROOT}/production.html"
