#!/bin/bash
# Update (or insert) a user's hub storage quota in the hubSpaceQuotas table
# on hgcentral (genome-centdb). Quota is in gigabytes.
#
# Shows the existing row, prompts for confirmation, runs the upsert,
# then shows the new row.
#
# Usage: updateHubSpaceQuota.sh <userName> <quotaGB>
# Example: updateHubSpaceQuota.sh gbUserName 50

set -euo pipefail

usage() {
    cat <<EOF
Usage: $(basename "$0") <userName> <quotaGB>

Update (or insert) a user's hub storage quota in hgcentral.hubSpaceQuotas.
Shows the existing row, prompts for confirmation, runs the upsert, then
shows the new row.

Example:
  $(basename "$0") gbUserName 50
EOF
}

if [ $# -eq 0 ]; then
    usage
    exit 0
fi

if [ $# -ne 2 ]; then
    usage >&2
    exit 1
fi

userName="$1"
quotaGB="$2"

if ! [[ "$quotaGB" =~ ^[0-9]+$ ]]; then
    echo "Error: quotaGB must be a positive integer (got '$quotaGB')" >&2
    exit 1
fi

if ! [[ "$userName" =~ ^[A-Za-z0-9_.-]+$ ]]; then
    echo "Error: userName contains unexpected characters (got '$userName')" >&2
    exit 1
fi

HGSQL="hgsql -h genome-centdb hgcentral"

echo ""
before=$($HGSQL -N -e \
    "select quota from hubSpaceQuotas where userName='${userName}'")
if [ -z "$before" ]; then
    echo "No existing row for userName='${userName}'"
    echo "Will INSERT new row with quota=${quotaGB} GB"
else
    echo "${userName} has a current quota of ${before} GB"
    echo "Will UPDATE quota: ${before} GB -> ${quotaGB} GB"
fi

echo
read -r -p "Proceed? [y/N] " reply
case "$reply" in
    y|Y|yes|YES)
        ;;
    *)
        echo "Aborted." >&2
        exit 1
        ;;
esac

sql="insert into hubSpaceQuotas (userName, quota) values ('${userName}', ${quotaGB}) \
on duplicate key update quota=${quotaGB}"
$HGSQL -e "$sql"

echo
echo "Done!"
echo "The account == ${userName} == now has ${quotaGB} GB."
