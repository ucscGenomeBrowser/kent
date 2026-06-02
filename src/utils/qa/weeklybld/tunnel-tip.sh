#!/bin/bash
#
# tunnel-tip.sh
#
# Open an ssh tunnel from this machine to the kent-tip docker container on
# hgwdev. After running, point a browser at http://localhost:8081 to reach the
# tip Genome Browser. Ctrl-C to close the tunnel.
# refs #37655
#
set -eu

HOST="${HGWDEV:-hgwdev.gi.ucsc.edu}"
PORT=8081

cat <<EOF
Opening ssh tunnel: localhost:$PORT -> $HOST kent-tip container.

While this terminal stays open, point your browser at:

    http://localhost:$PORT/cgi-bin/hgGateway

Other useful entry points:
    http://localhost:$PORT/cgi-bin/hgTracks
    http://localhost:$PORT/cgi-bin/hgsid

Leave this window running. Ctrl-C closes the tunnel.

EOF
exec ssh -N -L "$PORT:localhost:$PORT" "$HOST"
