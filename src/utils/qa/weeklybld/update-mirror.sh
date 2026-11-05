#!/bin/bash
#
# update-mirror.sh [--release] <browserSetup.sh>
#
# Update the long-lived kent-mirror container IN PLACE, the way a real mirror
# updates. autoBuild.sh runs it twice per release, both times with the new
# v${NN}_branch browserSetup.sh:
#   do_final   (default mode): cgiUpdate, then copy the beta CGIs over it, so
#              the new code meets the mirror's old state before QA starts
#   do_wrapup  (--release):    cgiUpdate only, no overlay -- exactly what a real
#              mirror gets once the release is on hgdownload
#
# The other instances (tip/beta/rel) are rebuilt or re-pulled every time, so each
# starts as a fresh install and never sees what a mirror sees after updating
# release after release: an hg.conf that cgiUpdate never touches, an hgcentral
# and MariaDB carried forward, files left over from older releases, and the
# browserSetup.sh update code path itself. kent-mirror exists to catch breakage
# there at build time, before the release reaches real mirrors.
#
# This is why the container is never recreated (refresh-instance.sh refuses it).
# It was created once from an old release image (v493, see run-instance.sh);
# everything since has been applied in place. To start its history over, use remove-instance.sh mirror.
#
# Steps:
#   1. create kent-mirror if it does not exist yet (first run only)
#   2. copy the given browserSetup.sh in and run `browserSetup.sh -b cgiUpdate`
#      -- the same software update mirrors run. At final time hgdownload still
#      serves the previous release's CGIs, so this exercises the new update
#      script, not the new binaries; those come next.
#   3. overlay the hgwdev beta CGIs, js and style (overlay-cgi.sh mirror), leaving
#      the container's hg.conf alone (skipped with --release)
#
# --release first checks that hgdownload already carries this build: its
# hgTracks must match the mtime of /usr/local/apache/cgi-bin-beta/hgTracks
# (the push preserves mtimes). If it does not, nothing is changed and the
# script exits 3, so the caller can say "re-run later" rather than "broken".
#
# Only cgiUpdate runs, not a full `update`: that would rsync the full hgFixed
# and every other database the container holds from hgdownload.
#
# Exits 3 if --release finds hgdownload not yet current, other non-zero if any
# step fails. The full log goes to stdout.
# refs #37655
#
set -eEu -o pipefail

selfDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
container=kent-mirror

usage() {
    echo "usage: $(basename "$0") [--release] <path to browserSetup.sh>" >&2
    exit 1
}

release=false
if [[ "${1:-}" == --release ]]; then
    release=true
    shift
fi
[[ $# -eq 1 ]] || usage
setupScript="$1"

if $release; then
    remote="$(rsync --list-only --no-motd hgdownload.soe.ucsc.edu::cgi-bin/hgTracks 2>/dev/null \
        | awk '{gsub("/", "-", $3); print $3, $4}')"
    remote="$(date -d "$remote" +%s 2>/dev/null || echo 0)"
    local_=$(stat -c %Y /usr/local/apache/cgi-bin-beta/hgTracks)
    if (( remote < local_ )); then
        echo "== hgdownload hgTracks ($(date -d @"$remote" '+%F %T')) is older than cgi-bin-beta's ($(date -d @"$local_" '+%F %T'))."
        echo "== The release has not reached hgdownload yet; kent-mirror left as is."
        echo "== Re-run later: $0 --release $setupScript"
        exit 3
    fi
fi
[[ -f "$setupScript" ]] || { echo "no such file: $setupScript" >&2; exit 1; }

if ! docker container inspect "$container" >/dev/null 2>&1; then
    echo "== $container does not exist; creating it from its seed release (see run-instance.sh)"
    "$selfDir/run-instance.sh" mirror
fi
if [[ "$(docker inspect -f '{{.State.Running}}' "$container")" != true ]]; then
    docker start "$container" >/dev/null
fi

# hg.conf before and after, so the log shows whether the update touched it
confSum() {
    docker exec "$container" md5sum /usr/local/apache/cgi-bin/hg.conf | cut -d' ' -f1
}
before="$(confSum)"

echo "== copying $setupScript into $container"
docker cp "$setupScript" "$container:/root/browserSetup.sh"
docker exec "$container" chmod a+x /root/browserSetup.sh

echo "== running browserSetup.sh -b cgiUpdate in $container"
docker exec "$container" /root/browserSetup.sh -b cgiUpdate

if $release; then
    echo "== release mode: no beta overlay, the CGIs are what hgdownload served"
else
    echo "== copying the hgwdev beta CGIs over the update"
    "$selfDir/overlay-cgi.sh" mirror
fi

after="$(confSum)"
if [[ "$before" == "$after" ]]; then
    echo "== hg.conf unchanged by the update (as on a real mirror)"
else
    echo "== NOTE: hg.conf changed during the update ($before -> $after)"
fi
# cgiUpdate restarts MariaDB; wait until the instance answers again so a smoke
# test run right after this does not race the restart
url="http://127.0.0.1:8085/cgi-bin/hgGateway"
start=$SECONDS
until [[ "$(curl -s -o /dev/null -m 20 -w '%{http_code}' "$url" || true)" == 200 ]]; do
    if (( SECONDS - start > 180 )); then
        echo "== $container is not serving $url 180s after the update" >&2
        exit 1
    fi
    sleep 5
done
echo "== $container updated and serving"
