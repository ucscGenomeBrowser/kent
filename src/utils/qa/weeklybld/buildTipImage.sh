#!/bin/bash
#
# buildTipImage.sh
#
# Build the local-only kent:tip docker image from the Dockerfile in
# src/product/installer/docker, then refresh the kent-tip container. amd64
# only; never pushed to Docker Hub. Run by hand or nightly from build's cron.
# refs #37655
#
set -eEu -o pipefail

selfDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKERDIR="$(cd "$selfDir/../../../product/installer/docker" && pwd)"
LOGDIR=~build/dockerStuff/logs
ALERT_EMAIL="${BUILDMEISTEREMAIL:-braney@ucsc.edu}"
mkdir -p "$LOGDIR"

# Demote the current :tip to :tip-previous so we have a rollback point if the
# new build fails or produces a broken image.
if docker image inspect kent:tip >/dev/null 2>&1; then
    docker tag kent:tip kent:tip-previous
fi

# The Dockerfile pulls master directly from GitHub, so any checkout builds an
# up-to-date tip image. Build amd64 only (hgwdev is amd64; we never push, so no
# arm64 / multi-arch manifest dance). On failure, roll back to :tip-previous so
# kent-tip keeps running the last good image, then alert.
if ! docker build --no-cache --platform linux/amd64 -t kent:tip "$DOCKERDIR" \
        >& "$LOGDIR/buildTipImage.log"; then
    if docker image inspect kent:tip-previous >/dev/null 2>&1; then
        docker tag kent:tip-previous kent:tip
    fi
    echo "buildTipImage.sh: docker build FAILED, rolled back to :tip-previous - see $LOGDIR/buildTipImage.log" | \
        mail -s "buildTipImage FAILED" "$ALERT_EMAIL"
    exit 1
fi

# Restart the kent-tip container against the new image.
"$selfDir/refresh-instance.sh" tip
