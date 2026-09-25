#!/bin/bash
# loadGnomadV4.1.1ChromTables.sh -- build and hgLoadSqlTab the per-chrom
# vcfTabix lookup tables (bbiChroms schema) for the gnomAD v4.1.1
# exomes and genomes vcfTabix children. Idempotent: rerunning replaces
# the table contents.
#
# Usage: loadGnomadV4.1.1ChromTables.sh [DB]
#   DB defaults to hg38.
#
# Pre-req: /gbdb symlinks at /gbdb/hg38/gnomAD/v4.1.1/{exomes,genomes}/
# already in place.

set -euo pipefail

DB="${1:-hg38}"
SCHEMA=$HOME/kent/src/hg/lib/bbiChroms.sql
WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

CHROMS="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 X Y"

for ds in exomes genomes; do
    table="gnomad${ds^}V4_1_1Vcf"       # gnomadExomesV4_1_1Vcf / gnomadGenomesV4_1_1Vcf
    tsv="${WORKDIR}/${table}.txt"
    : > "$tsv"
    for chr in $CHROMS; do
        printf '/gbdb/%s/gnomAD/v4.1.1/%s/gnomad.%s.v4.1.1.sites.chr%s.vcf.bgz\tchr%s\n' \
            "$DB" "$ds" "$ds" "$chr" "$chr" >> "$tsv"
    done
    echo "Loading $table from $tsv ($(wc -l < "$tsv") rows)"
    hgLoadSqlTab "$DB" "$table" "$SCHEMA" "$tsv"
done

echo "done"
