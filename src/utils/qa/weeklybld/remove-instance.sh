#!/bin/bash
#
# remove-instance.sh <tip|beta|beta-arm64|rel>
#
# Stop and remove the named container. For beta and beta-arm64, also remove the
# local image and named volumes, since the beta instances are torn down for good
# when a release ships (do_wrapup) and the next build cycle starts from a clean
# slate.
#
# A release instance (v499, v503, ...) loses its volumes too, because removing
# it means we have stopped keeping that release. Its image stays: it is the
# published release image, so a later run-instance.sh needs no pull. refs #38377
# refs #37655
#
set -eEu -o pipefail

usage() {
    echo "usage: $(basename "$0") tip|beta|rel|vNNN" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
name="$1"
case "$name" in
    tip|beta|beta-arm64|rel|v[0-9][0-9][0-9]) ;;
    *)                                        usage ;;
esac
container="kent-$name"

docker stop "$container" >/dev/null 2>&1 || true
docker rm   "$container" >/dev/null 2>&1 || true

case "$name" in
    beta|beta-arm64)
        docker rmi "kent:$name" >/dev/null 2>&1 || true
        docker volume rm "kent-${name}-mysql" "kent-${name}-gbdb" >/dev/null 2>&1 || true
        ;;
    v[0-9][0-9][0-9])
        docker volume rm "kent-${name}-mysql" "kent-${name}-gbdb" >/dev/null 2>&1 || true
        ;;
esac
