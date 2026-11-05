#!/bin/bash
#
# tunnel-release.sh <vNNN>
#
# Open an ssh tunnel from this machine to a past-release docker container on
# hgwdev, e.g. tunnel-release.sh v503. After running, point a browser at
# http://localhost:8NNN to reach the Genome Browser exactly as that release
# shipped it. Ctrl-C to close the tunnel.
#
# There is one script for every release rather than one per release, because
# which releases we keep changes every few weeks. The port encodes the release
# the same way run-instance.sh assigns it: v503 -> 8503.
# refs #38377
#
set -eu

usage() {
    echo "usage: $(basename "$0") vNNN    (e.g. $(basename "$0") v503)" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
name="$1"
[[ "$name" =~ ^v[0-9][0-9][0-9]$ ]] || usage

HOST="${HGWDEV:-hgwdev.gi.ucsc.edu}"
PORT=$(( 8000 + 10#${name#v} ))

cat <<EOF2
Opening ssh tunnel: localhost:$PORT -> $HOST kent-$name container.

While this terminal stays open, point your browser at:

    http://localhost:$PORT/cgi-bin/hgGateway

Other useful entry points:
    http://localhost:$PORT/cgi-bin/hgTracks
    http://localhost:$PORT/cgi-bin/hgsid

Leave this window running. Ctrl-C closes the tunnel.

EOF2
exec ssh -N -L "$PORT:localhost:$PORT" "$HOST"
