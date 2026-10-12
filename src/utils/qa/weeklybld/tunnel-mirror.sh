#!/bin/bash
#
# tunnel-mirror.sh
#
# Open an ssh tunnel from this machine to the kent-mirror docker container on
# hgwdev. After running, point a browser at http://localhost:8085 to reach a
# Genome Browser that has been updated in place like a real mirror, release
# after release, since v493 (see update-mirror.sh). Ctrl-C to close the tunnel.
# refs #38514
#
set -eu

HOST="${HGWDEV:-hgwdev.gi.ucsc.edu}"
PORT=8085

cat <<EOF2
Opening ssh tunnel: localhost:$PORT -> $HOST kent-mirror container.

While this terminal stays open, point your browser at:

    http://localhost:$PORT/cgi-bin/hgGateway

Other useful entry points:
    http://localhost:$PORT/cgi-bin/hgTracks
    http://localhost:$PORT/cgi-bin/hgsid

Leave this window running. Ctrl-C closes the tunnel.

EOF2
exec ssh -N -L "$PORT:localhost:$PORT" "$HOST"
