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
# For tip, beta and rel the named volumes are removed, so the new container
# starts from the image's own database.
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
    # Recreating kent-mirror would throw away the in-place-updated cgi-bin,
    # htdocs and hg.conf that are the whole point of it. Use update-mirror.sh,
    # or remove-instance.sh mirror to deliberately start it over.
    mirror)              echo "refusing to recreate kent-mirror; use update-mirror.sh" >&2; exit 1 ;;
    *)                   usage ;;
esac
container="kent-$name"

docker stop "$container" >/dev/null 2>&1 || true
docker rm   "$container" >/dev/null 2>&1 || true

# Docker seeds a named volume from the image only when it creates the volume,
# so a kept volume serves the MariaDB of whatever image first made it. kent-rel
# ran the v504 image against a hgcentral from May 2026 that had no assemblyList
# table. Drop the volumes so these instances get the database of the image they
# run. A release instance keeps its volumes: its image never changes, and the
# v499-v503 volumes carry a hand-rebuilt full-text index. refs #38387
case "$name" in
    tip|beta|beta-arm64|rel)
        docker volume rm "kent-${name}-mysql" "kent-${name}-gbdb" >/dev/null 2>&1 || true
        ;;
esac

exec "$selfDir/run-instance.sh" "$name"
