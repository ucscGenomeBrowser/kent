#!/bin/bash
#
# trackLists.sh - build the mirror-facing page listing tracks we cannot
# redistribute, tracks that update themselves, and contributed tracks. RM #37781
#
# Cron (otto): once a week is plenty; none of these lists move daily.
#     32 6 * * 4 /hive/data/outside/otto/trackLists/trackLists.sh
#
# Writes the public page to htdocs on hgwdev. Getting it to the RR needs a
# /root/<name>AutoPush entry in /etc/crontab, which only cluster-admin can add
# (same pattern as the daily tips and the session thumbnails, which are likewise
# generated into htdocs and are not tracked in git).
#
# Output behaviour: quiet on success, except that a restricted file found to be
# reachable on hgdownload always prints, so cron mails it.

set -o errexit -o pipefail
umask 002

DIR=${RTDIR:-/hive/data/outside/otto/trackLists}
HTDOCS=${HTDOCS:-/usr/local/apache/htdocs}   # override for testing
PAGE=trackLists.html

cd "$DIR"

# --refresh-contrib is implicit: the crawl re-runs itself when the cache ages out
./collect.py --cache "$DIR/cache" -o "$DIR/collected.json"

# public page: no list of reachable restricted files
./mkPage.py -i "$DIR/collected.json" -o "$DIR/$PAGE"

# internal copy, keeps the hgdownload cross-check, stays on hgwdev
./mkPage.py -i "$DIR/collected.json" -o "$DIR/internal.html" --internal

# only replace the live page if it actually changed
if ! cmp -s "$DIR/$PAGE" "$HTDOCS/$PAGE"; then
    cp -p "$DIR/$PAGE" "$HTDOCS/$PAGE"
    echo "trackLists: updated $HTDOCS/$PAGE"
fi
