#!/bin/bash
#
# refresh-instance.sh <tip|beta|rel>
#
# Stop and remove the named container, then start a fresh one from the current
# image. For rel, pull genomebrowser/server:latest from Docker Hub first; tip
# and beta are built locally on hgwdev so there is nothing to pull. Persistent
# state under ~build/dockerStuff/state/<name> survives because it is on host
# volumes.
# refs #37655
#
set -eEu -o pipefail

selfDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    echo "usage: $(basename "$0") tip|beta|rel" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
name="$1"
case "$name" in
    tip|beta|beta-arm64) ;;
    rel)                 docker pull genomebrowser/server:latest ;;
    *)                   usage ;;
esac
container="kent-$name"

docker stop "$container" >/dev/null 2>&1 || true
docker rm   "$container" >/dev/null 2>&1 || true

exec "$selfDir/run-instance.sh" "$name"
