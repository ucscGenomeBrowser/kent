#!/bin/bash
#
# tunnel-rel.sh
#
# Open an ssh tunnel from this machine to the kent-rel docker container on
# hgwdev. After running, point a browser at http://localhost:8083 to reach the
# released Genome Browser (the same image mirrors run). Ctrl-C to close the
# tunnel.
# refs #37655
#
set -eu

HOST="${HGWDEV:-hgwdev.gi.ucsc.edu}"
PORT=8083

cat <<EOF
Opening ssh tunnel: localhost:$PORT -> $HOST kent-rel container.

While this terminal stays open, point your browser at:

    http://localhost:$PORT/cgi-bin/hgGateway

Other useful entry points:
    http://localhost:$PORT/cgi-bin/hgTracks
    http://localhost:$PORT/cgi-bin/hgsid

Leave this window running. Ctrl-C closes the tunnel.

EOF
exec ssh -N -L "$PORT:localhost:$PORT" "$HOST"
