#!/bin/bash

# Installs  pre-commit hook that ensures attempts to commit files that are
# larger than $limit to your _local_ repo fail, with a helpful error message.
cd "$(dirname "$0")"   # location of this script
cd .git/hooks/
ln -sf ../../pre-commit-prevent-large-files.sh pre-commit
