#!/bin/bash
#
# stop-all.sh - stop all running docker browser containers (tip, beta,
# beta-arm64, rel). A manually stopped container stays stopped across daemon
# restarts; use start-all.sh to bring them back.
# refs #37655
#
set -eu

for name in tip beta beta-arm64 rel; do
    c="kent-$name"
    if docker container inspect "$c" >/dev/null 2>&1; then
        docker stop "$c" >/dev/null && echo "stopped $c"
    fi
done
