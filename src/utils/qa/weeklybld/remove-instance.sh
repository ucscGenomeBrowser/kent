#!/bin/bash
#
# remove-instance.sh <tip|beta|rel>
#
# Stop and remove the named container. For beta, also remove the local
# kent:beta image and its named volumes, since beta is torn down for good when
# a release ships (do_wrapup) and the next build cycle starts from a clean
# slate.
# refs #37655
#
set -eEu -o pipefail

usage() {
    echo "usage: $(basename "$0") tip|beta|rel" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
name="$1"
case "$name" in
    tip|beta|rel) ;;
    *)            usage ;;
esac
container="kent-$name"

docker stop "$container" >/dev/null 2>&1 || true
docker rm   "$container" >/dev/null 2>&1 || true

if [[ "$name" == beta ]]; then
    docker rmi kent:beta >/dev/null 2>&1 || true
    docker volume rm kent-beta-mysql kent-beta-gbdb >/dev/null 2>&1 || true
fi
