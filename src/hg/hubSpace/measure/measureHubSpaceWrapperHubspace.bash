#!/bin/bash

# weekly cron wrapper for measureHubSpace.sh, which writes its own dated report
# under /home/qateam/hubSpace/YYYY/MM/.  The hgwdev wrapper copies the reports
# to /hive/users/qateam/hubSpace/hubspace/ and draws the charts page there.
#
# crontab entry, Mondays at 06:00:
# 0 6 * * 1 /home/qateam/hubSpace/measureHubSpaceWrapperHubspace.bash

set -beEu -o pipefail

MEASURE="/home/qateam/hubSpace/measureHubSpace.sh"

"${MEASURE}"
