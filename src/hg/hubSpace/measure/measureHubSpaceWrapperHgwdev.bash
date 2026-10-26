#!/bin/bash

# weekly cron wrapper for measureHubSpace.dev.sh, which writes its own dated
# report under ~/hubSpace/YYYY/MM/, then hubSpaceReport.py redraws the charts
# page from all of the reports
#
# crontab entry, Mondays at 06:00:
# 0 6 * * 1 /cluster/home/chmalee/bin/measureHubSpaceWrapperHgwdev.bash

set -beEu -o pipefail

MEASURE="/cluster/home/chmalee/hubSpace/measureHubSpace.dev.sh"
REPORT="/cluster/home/chmalee/hubSpace/hubSpaceReport.py"
LOGROOT="/cluster/home/chmalee/hubSpace"
PAGEDIR="/cluster/home/chmalee/public_html/hubSpace"

"${MEASURE}"

mkdir -p "${PAGEDIR}"
"${REPORT}" -l hgwdev "${LOGROOT}" "${PAGEDIR}/hgwdev.html"
