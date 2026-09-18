#!/bin/bash
# methbaseOtto.sh - monthly refresh of the MethBase2 mirror and of the trackDb
# stanzas generated from it.
#
# Two machines are involved. The 6 TB of data lives on hgdownload and is
# refreshed there, in place, by methbaseDownload - we do not copy it through
# hgwdev. Only the resulting trackDb text comes back here, as the subtrack file
# that hg38's methbase2.ra includes.
#
#   1. ssh to hgdownload and run methbaseDownload against the mirror. It HEADs
#      every file and fetches only what changed upstream, so a month with no
#      new data costs one pass over the file list and no downloads.
#   2. pull this kent tree.
#   3. fetch the mirror's hg38 trackDb, drop the composite header (everything
#      through the first blank line) and keep the subtrack stanzas.
#   4. if that differs from what is committed, commit and push it.
#
# Silent when nothing changed, which is the otto contract. Any failure aborts
# the run and prints to stderr, so cron mails it.
#
# The composite header itself is NOT generated: it stays hand-maintained in
# methbase2.ra, because it carries the /gbdb metaDataUrl and colorSettingsUrl
# paths and the dnaMethylation parent, none of which exist in the upstream hub.
#
# Edit this in the kent tree, then copy it to
# /hive/data/outside/otto/methbase2/ - cron runs the hive copy.
#
# Crontab line for otto, 03:00 on the first of the month:
#   0 3 1 * * /hive/data/outside/otto/methbase2/methbaseOtto.sh
#
# Needs, as the otto user: an ssh key accepted by $DOWNLOAD_HOST, a kent tree at
# $KENT whose remote it may push to, and methbaseDownload deployed at
# $REMOTE_SCRIPT on hgdownload (it needs python3 with the requests module).

set -o errexit -o nounset -o pipefail

# hgdownload account and the mirror it serves. The vN directory appears in the
# URL too, so bump both together when a new version is cut.
DOWNLOAD_HOST=${DOWNLOAD_HOST:-qateam@hgdownload}
MIRROR_DIR=${MIRROR_DIR:-/mirrordata/hubs/methbase/v3}
MIRROR_URL=${MIRROR_URL:-https://hgdownload.soe.ucsc.edu/hubs/methbase/v3}
# methbaseDownload as deployed on hgdownload, not the copy in this directory
REMOTE_SCRIPT=${REMOTE_SCRIPT:-/mirrordata/hubs/methbase/methbaseDownload}

KENT=${KENT:-$HOME/kent}
RA=$KENT/src/hg/makeDb/trackDb/human/hg38/methbase2-subtracks.ra

# a subtrack stanza is 8-10 lines, so this is a floor well under any real hub;
# it exists to catch a truncated or error-page download, not to police content
MIN_LINES=1000

fail() {
    echo "methbaseOtto.sh: $*" >&2
    exit 1
}
trap 'fail "aborted at line $LINENO"' ERR

# 1. refresh the mirror on hgdownload. Its own log lives there; we only care
# whether it succeeded, and it exits non-zero if any file failed or an assembly
# had to be skipped.
ssh -o BatchMode=yes "$DOWNLOAD_HOST" "$REMOTE_SCRIPT -o $MIRROR_DIR" \
    || fail "mirror update failed on $DOWNLOAD_HOST - see $MIRROR_DIR/download.log"

# 2. a stale tree would make the diff and the push meaningless
cd "$KENT"
git pull --ff-only --quiet || fail "git pull failed in $KENT"

# 3. the header runs to the first blank line; everything after it is subtracks
new=$(mktemp) || fail "mktemp failed"
trap 'rm -f "$new"' EXIT
curl -sSf "$MIRROR_URL/hub/hg38/trackDb.txt" | sed '1,/^$/d' > "$new" \
    || fail "could not fetch hg38 trackDb from $MIRROR_URL"

# guard against committing a truncated fetch or an error page
lines=$(wc -l < "$new")
[ "$lines" -ge "$MIN_LINES" ] || fail "hg38 trackDb looks wrong: only $lines lines"
head -1 "$new" | grep -q '^track ' \
    || fail "hg38 trackDb does not start with a track stanza after the header"
grep -q "^bigDataUrl $MIRROR_URL/data/" "$new" \
    || fail "hg38 trackDb bigDataUrls do not point at $MIRROR_URL/data/"

# 4. nothing to do is the normal monthly outcome, and it stays silent
if cmp -s "$new" "$RA"; then
    exit 0
fi

changed=$(diff "$RA" "$new" | grep -c '^[<>]' || true)
added=$(diff "$RA" "$new" | grep -c '^>' || true)
removed=$(diff "$RA" "$new" | grep -c '^<' || true)

cp "$new" "$RA"
git add "$RA"
git commit --no-verify --quiet -m \
    "hg38 Methbase: refresh subtracks from the mirror, $changed lines changed ($added added, $removed removed), refs #34246" \
    -- "$RA" || fail "git commit failed"
git push --quiet || fail "git push failed"

# an update happened, so say so - this is the one case that should mail
echo "MethBase2: hg38 subtracks updated, $changed lines changed ($added added, $removed removed)"
echo "committed $(git rev-parse --short HEAD) and pushed"
