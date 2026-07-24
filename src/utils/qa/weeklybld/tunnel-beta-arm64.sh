#!/bin/bash
#
# tunnel-beta-arm64.sh
#
# Open an ssh tunnel from this machine to the kent-beta-arm64 docker container
# on hgwdev. After running, point a browser at http://localhost:8084 to reach
# the arm64 beta Genome Browser (an arm64 image compiled from v<NN>_branch and
# run emulated on the amd64 host). Note kent-beta-arm64 only exists during a
# release cycle (it comes up at autoBuild.sh do_final and is torn down at the
# next do_wrapup); outside that window this tunnel will connect but the
# container won't be reachable. Ctrl-C to close the tunnel.
# refs #37655
#
set -eu

HOST="${HGWDEV:-hgwdev.gi.ucsc.edu}"
PORT=8084

cat <<EOF
Opening ssh tunnel: localhost:$PORT -> $HOST kent-beta-arm64 container.

While this terminal stays open, point your browser at:

    http://localhost:$PORT/cgi-bin/hgGateway

Other useful entry points:
    http://localhost:$PORT/cgi-bin/hgTracks
    http://localhost:$PORT/cgi-bin/hgsid

Note: kent-beta-arm64 only exists during a release cycle (it comes up at
autoBuild.sh do_final and is torn down at the next do_wrapup). Outside that
window the tunnel will connect but pages will return connection-refused.

Leave this window running. Ctrl-C closes the tunnel.

EOF
exec ssh -N -L "$PORT:localhost:$PORT" "$HOST"
