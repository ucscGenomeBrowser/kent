#!/bin/bash
# Build a probeId -> hg38 coordinate table from the Illumina methylation array
# manifests that are already loaded as tracks on the UCSC browser.
#
# EPIC v1 (850K) is preferred because nearly all published episignatures were
# derived on it or on its 450K subset. EPIC v2 fills in probes that v1 does not
# carry, 450K is checked last (it is a strict subset of v1 here and never adds
# anything, but it is cheap to include and guards against a future manifest swap).
#
# Output: probeId, chrom, chromStart, chromEnd, source   (tab separated)
set -beEu -o pipefail

out=${1:-probeCoords.tsv}
bbi=/gbdb/hg38/bbi/illumina

tmp=$(mktemp -d)
trap "rm -rf $tmp" EXIT

# EPIC v2 item names carry a replicate suffix (cg00381604_BC11); field 13 holds
# the bare probe ID, which is what the episignature studies report.
bigBedToBed $bbi/illuminaEPICv2.bb stdout \
    | awk -F'\t' 'BEGIN{OFS="\t"} {print $13,$1,$2,$3,"EPICv2"}' | sort -u > $tmp/v2
bigBedToBed $bbi/epic850K.bb stdout \
    | awk -F'\t' 'BEGIN{OFS="\t"} {print $4,$1,$2,$3,"EPIC850K"}' | sort -u > $tmp/v1
bigBedToBed $bbi/illumina450K.bb stdout \
    | awk -F'\t' 'BEGIN{OFS="\t"} {print $4,$1,$2,$3,"HM450K"}' | sort -u > $tmp/v450

# first file wins per probe ID
awk -F'\t' '!seen[$1]++' $tmp/v1 $tmp/v2 $tmp/v450 | sort -k1,1 > $out
wc -l $out
