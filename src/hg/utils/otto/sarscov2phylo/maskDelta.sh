#!/bin/bash
set -beEu -x -o pipefail

# NOTE: in case you found this script by following a github link from a discussion about
# branch-specific masking in the UShER SARS-CoV-2 tree, the old contents of this script
# have been replaced by a data file describing the branches and masked sites:
#   branchSpecificMask.yml
# and a script that interprets the data file and applies the masking:
#   branchSpecificMask.py
# See discussion in https://github.com/yatisht/usher/issues/324

usage() {
    echo "usage: $0 treeIn.pb treeOut.pb"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

treeInPb=$1
treeOutPb=$2

scriptDir=$(dirname "${BASH_SOURCE[0]}")
export PATH=~angie/github/usher/build:"$PATH"

$scriptDir/branchSpecificMask.py $treeInPb $scriptDir/branchSpecificMask.yml $treeOutPb
