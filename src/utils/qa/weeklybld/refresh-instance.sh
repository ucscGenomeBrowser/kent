#!/bin/bash
#
# refresh-instance.sh <tip|beta|rel|vNNN>
#
# Stop and remove the named container, then start a fresh one from the current
# image. For rel and for a release instance (v499, v503, ...), pull the image
# from Docker Hub first; tip and beta are built locally on hgwdev so there is
# nothing to pull. A release tag never moves, so its pull is a no-op unless the
# image is missing locally. Persistent state under
# ~build/dockerStuff/state/<name> survives because it is on host volumes.
# refs #38377
# refs #37655
#
set -eEu -o pipefail

selfDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    echo "usage: $(basename "$0") tip|beta|rel|vNNN" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
name="$1"
case "$name" in
    tip|beta|beta-arm64) ;;
    rel)                 docker pull genomebrowser/server:latest ;;
    v[0-9][0-9][0-9])    docker pull "genomebrowser/server:$name" ;;
    *)                   usage ;;
esac
container="kent-$name"

docker stop "$container" >/dev/null 2>&1 || true
docker rm   "$container" >/dev/null 2>&1 || true

exec "$selfDir/run-instance.sh" "$name"
