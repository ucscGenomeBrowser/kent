#!/bin/bash
# Process one HPRC2 per-assembly "vs GRCh38" chain into the intermediates used by
# the hg38 native tracks (chain/net, bigChain, rearrangements, indels).
# The raw chains have GRCh38 as the QUERY, so we chainSwap to put hg38 as TARGET.
# Query chrom.sizes are derived from the chain headers (no assembly 2bit needed).
# Redmine #35415
# Usage: hprc2ProcessOne.sh <rawChain.gz> <shortName> <bedDir>

set -beEu -o pipefail
export PATH=/cluster/bin/x86_64:/cluster/bin/scripts:$PATH
ulimit -s unlimited || true

raw="${1}"
name="${2}"
BED="${3}"
hgSizes=/hive/data/genomes/hg38/chrom.sizes

chainF="${BED}/chain/${name}.chain"
netF="${BED}/net/${name}.net"
qSizes="${BED}/qSizes/${name}.qSizes"
bcF="${BED}/bigChains/${name}.bb"

mkdir -p "${BED}/chain" "${BED}/net" "${BED}/qSizes" "${BED}/bigChains" \
         "${BED}/arr" "${BED}/tab"

# 1. swap (GRCh38 -> target hg38), sort, renumber chain ids
if [ ! -s "${chainF}" ]; then
  zcat "${raw}" | chainSwap stdin stdout | chainSort stdin stdout \
    | awk '/^chain/{$13=++n} {print}' > "${chainF}"
fi

# 2. query (assembly) chrom.sizes straight from chain headers
awk '/^chain/{print $8"\t"$9}' "${chainF}" | sort -u > "${qSizes}"

# 3. net, syntenic-classified (required for netChainSubset -type=top)
if [ ! -s "${netF}" ]; then
  chainPreNet "${chainF}" "${hgSizes}" "${qSizes}" stdout \
    | chainNet -minSpace=1 stdin "${hgSizes}" "${qSizes}" stdout /dev/null \
    | netSyntenic stdin "${netF}"
fi

# 4. bigChain (+ link) for the per-assembly chain track, breaks and coverage
# hgLoadChain -test writes chain.tab/link.tab into the CURRENT directory, so each
# parallel job needs its own working dir to avoid clobbering the others.
work="${BED}/tab/${name}.work"
rm -rf "${work}"; mkdir -p "${work}"
( cd "${work}"
  hgLoadChain -noBin -test hg38 bigChain "${chainF}" >/dev/null 2>&1
  sed 's/\.000000//' chain.tab \
    | awk 'BEGIN{OFS="\t"}{print $2,$4,$5,$11,1000,$8,$3,$6,$7,$9,$10,$1}' \
    | sort -k1,1 -k2,2n > bigChain.bed
  bedToBigBed -type=bed6+6 -as=$HOME/kent/src/hg/lib/bigChain.as -tab \
    bigChain.bed "${hgSizes}" "${bcF}" 2>/dev/null
  awk 'BEGIN{OFS="\t"}{print $1,$2,$3,$5,$4}' link.tab \
    | sort -k1,1 -k2,2n > bigLink.bed
  bedToBigBed -type=bed4+1 -as=$HOME/kent/src/hg/lib/bigLink.as -tab \
    bigLink.bed "${hgSizes}" "${BED}/bigChains/${name}.link.bb" 2>/dev/null
)
rm -rf "${work}"

# 5. rearrangements: inversions + duplications (per-assembly raw)
chainArrange "${chainF}" "${name}" "${BED}/arr/${name}.inv.txt" \
  "${BED}/arr/${name}.dup.txt" 2>/dev/null

# 6. indels from top-level chains of the net
topChain="${BED}/arr/${name}.top.chain"
netChainSubset "${netF}" "${chainF}" "${topChain}" -type=top 2>/dev/null
chainInDel "${topChain}" "${name}" "${BED}/arr/${name}.indel.txt" 2>/dev/null
rm -f "${topChain}"

echo "OK ${name}"
