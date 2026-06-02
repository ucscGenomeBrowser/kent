#!/bin/bash
#
# start-all.sh - start the docker browser containers that exist (tip, rel, and
# beta when present). Use after a manual stop-all; on reboot the containers'
# --restart=unless-stopped policy brings them back on their own.
# refs #37655
#
set -eu

for name in tip rel beta; do
    c="kent-$name"
    if docker container inspect "$c" >/dev/null 2>&1; then
        docker start "$c" >/dev/null && echo "started $c"
    fi
done
