#!/bin/bash
#
# tunnel-beta.sh
#
# Open an ssh tunnel from this machine to the kent-beta docker container on
# hgwdev. After running, point a browser at http://localhost:8082 to reach the
# beta Genome Browser. Note kent-beta only exists during a release cycle (it
# comes up at autoBuild.sh do_final and is torn down at the next do_wrapup);
# outside that window this tunnel will connect but the container won't be
# reachable. Ctrl-C to close the tunnel.
# refs #37655
#
set -eu

HOST="${HGWDEV:-hgwdev.gi.ucsc.edu}"
PORT=8082

cat <<EOF
Opening ssh tunnel: localhost:$PORT -> $HOST kent-beta container.

While this terminal stays open, point your browser at:

    http://localhost:$PORT/cgi-bin/hgGateway

Other useful entry points:
    http://localhost:$PORT/cgi-bin/hgTracks
    http://localhost:$PORT/cgi-bin/hgsid

Note: kent-beta only exists during a release cycle (it comes up at autoBuild.sh
do_final and is torn down at the next do_wrapup). Outside that window the
tunnel will connect but pages will return connection-refused.

Leave this window running. Ctrl-C closes the tunnel.

EOF
exec ssh -N -L "$PORT:localhost:$PORT" "$HOST"
