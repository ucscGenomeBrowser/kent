#!/bin/bash
# Regenerate the per-assembly indel intermediate for the hprc2X track set, this
# time left-normalizing pure deletions against the hg38 target sequence
# (chainInDel -t2bit).  Everything else the hprc2 build produced (chain/net,
# bigChains, inversions, duplications) is reused unchanged from the hprc2 workdir,
# so this step only redoes the netChainSubset -> chainInDel indel extraction.
# Redmine #35415
# Usage: hprc2XIndelsOne.sh <shortName> <hprc2Dir> <hprc2XDir>

set -beEu -o pipefail
# put the user build of chainInDel (with -t2bit support) ahead of the system copy
export PATH=$HOME/bin/x86_64:/cluster/bin/x86_64:/cluster/bin/scripts:$PATH
ulimit -s unlimited || true

name="${1}"
SRC="${2}"                 # existing hprc2 workdir (chain/, net/ reused)
BED="${3}"                 # hprc2X workdir
t2bit=/hive/data/genomes/hg38/hg38.2bit

chainF="${SRC}/chain/${name}.chain"
netF="${SRC}/net/${name}.net"
mkdir -p "${BED}/arr"

topChain="${BED}/arr/${name}.top.chain"
netChainSubset "${netF}" "${chainF}" "${topChain}" -type=top 2>/dev/null
$HOME/bin/x86_64/chainInDel -t2bit="${t2bit}" "${topChain}" "${name}" \
  "${BED}/arr/${name}.indel.txt" 2>/dev/null
rm -f "${topChain}"

echo "OK ${name}"
