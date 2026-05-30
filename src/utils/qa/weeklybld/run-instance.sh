#!/bin/bash
#
# run-instance.sh <tip|beta|rel>
#
# Start one of the three docker browser instances on hgwdev. The image is fully
# self-contained: MariaDB, Apache and the CGIs were baked in at build time by
# browserSetup.sh, so NOTHING from the hgwdev filesystem is bind-mounted into
# the container. Anything that must survive a refresh (the MariaDB data dir and
# /gbdb) lives on Docker-managed named volumes, which seed from the image's
# baked content on first use and persist across refresh-instance cycles. For
# tip and beta, the matching hgwdev CGIs are copied in on top afterward (see
# overlay-cgi.sh) -- a copy, not a mount.
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
    tip)  port=8081; image=kent:tip ;;
    beta) port=8082; image=kent:beta ;;
    rel)  port=8083; image=genomebrowser/server:latest ;;
    *)    usage ;;
esac
container="kent-$name"

# -p 127.0.0.1:PORT:80 binds to loopback only, so the container is reachable
#   from hgwdev itself (reverse-proxy vhosts or an ssh tunnel), not the network.
# --restart=unless-stopped brings the container back after a daemon restart or
#   reboot without manual intervention.
# --memory/--cpus cap resource use so a runaway query can't starve hgwdev.
docker run -d --name "$container" \
    --restart=unless-stopped \
    --memory=8g --cpus=2 \
    -p "127.0.0.1:${port}:80" \
    -v "kent-${name}-mysql:/var/lib/mysql" \
    -v "kent-${name}-gbdb:/gbdb" \
    "$image"

# tip and beta ship the public release CGIs from the image; overlay the
# matching hgwdev CGIs (master for tip, branch beta for beta) on top.
case "$name" in
    tip|beta) "$selfDir/overlay-cgi.sh" "$name" ;;
esac
