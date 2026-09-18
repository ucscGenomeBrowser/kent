#!/bin/bash
#
# start-all.sh - start the docker browser containers that exist (tip, rel, the
# beta / beta-arm64 pair when present, and every past-release instance). Use
# after a manual stop-all; on reboot the containers' --restart=unless-stopped
# policy brings them back on their own.
# refs #37655
#
set -eu

# The release instances are whatever kent-vNNN containers exist, rather than a
# list to keep in step with how many releases we are keeping. refs #38377
names=(tip rel beta beta-arm64)
while read -r c; do
    names+=("${c#kent-}")
done < <(docker ps -a --format '{{.Names}}' | grep -E '^kent-v[0-9]{3}$' | sort)

for name in "${names[@]}"; do
    c="kent-$name"
    if docker container inspect "$c" >/dev/null 2>&1; then
        docker start "$c" >/dev/null && echo "started $c"
    fi
done
