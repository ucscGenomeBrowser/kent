#!/bin/bash
#
# stop-all.sh - stop all running docker browser containers (tip, beta,
# beta-arm64, rel, and every past-release instance). A manually stopped
# container stays stopped across daemon restarts; use start-all.sh to bring them
# back.
# refs #37655
#
set -eu

# The release instances are whatever kent-vNNN containers exist. refs #38377
names=(tip beta beta-arm64 rel)
while read -r c; do
    names+=("${c#kent-}")
done < <(docker ps -a --format '{{.Names}}' | grep -E '^kent-v[0-9]{3}$' | sort)

for name in "${names[@]}"; do
    c="kent-$name"
    if docker container inspect "$c" >/dev/null 2>&1; then
        docker stop "$c" >/dev/null && echo "stopped $c"
    fi
done
