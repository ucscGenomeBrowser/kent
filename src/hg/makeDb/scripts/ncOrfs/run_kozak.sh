#!/bin/bash
# Recolor an ncORF bigBed by Kozak strength and convert to bigGenePred.
# Usage:  run_kozak.sh <inFmt: bigGenePred|bed12> <inBb> <asFile> <outBb>
# Example:
#   run_kozak.sh bigGenePred Ribo-seq_ORFs.bb gencNcOrf.as Ribo-seq_ORFs.kozak.bb

set -euo pipefail

INFMT=${1:?inFmt required}
INBB=${2:?inBb required}
ASFILE=${3:?asFile required}
OUTBB=${4:?outBb required}

SCRIPTDIR=$(cd "$(dirname "$0")" && pwd)
TWOBIT=${TWOBIT:-/hive/data/genomes/hg38/hg38.2bit}
CHROMSIZES=${CHROMSIZES:-/hive/data/genomes/hg38/chrom.sizes}
TE=${TE:-$SCRIPTDIR/translational_efficiency.txt}

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

BED=$TMPDIR/out.bed
REPORT=${OUTBB%.bb}.report.tsv

bigBedToBed "$INBB" stdout \
  | python3 "$SCRIPTDIR/colorByKozak.py" \
        --twoBit "$TWOBIT" \
        --teTable "$TE" \
        --inFmt "$INFMT" \
        --report "$REPORT" \
  | sort -k1,1 -k2,2n \
  > "$BED"

# Field count = 12 + extra fields; we pass through input extras and append 3
# (startCodon, kozakStrength, kozakTE). For the gencNcOrf bigGenePred input
# (12+14 extras) the output is 12+17.
NF=$(awk -F'\t' '{print NF; exit}' "$BED")
EXTRA=$((NF - 12))

bedToBigBed -type=bed12+${EXTRA} -as="$ASFILE" -tab \
            "$BED" "$CHROMSIZES" "$OUTBB"

echo "Wrote $OUTBB ($NF fields, $EXTRA extra)"
echo "Report: $REPORT"
