#!/bin/bash

set -o pipefail -o errexit -e

export PATH="${PATH}:/cluster/bin/x86_64"

cd /hive/data/outside/otto/civic

/cluster/software/bin/uv run civicToBed.py
