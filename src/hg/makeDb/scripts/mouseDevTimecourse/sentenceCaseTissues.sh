#!/bin/bash
# Sentence-case the tissue names and the facet column titles in the Wold Lab
# mouse development .facets and .categories files, so the barChart facet filter
# reads "Tissue / Spleen" rather than "tissue / spleen".
#
# Only the first character is upper-cased: "skeletal muscle tissue" becomes
# "Skeletal muscle tissue", not Title Case.
#
# What is changed:
#   .facets     header row  - the tissue and timepoint column names only
#               data rows   - the label (col 1) and the tissue (col 3)
#   .categories data rows   - the label (col 1), which must stay identical to
#                             the .facets label column
#
# What is deliberately NOT changed: the count and color column names. hgTracks
# matches those by exact lower-case string - barChartUi.c requires a field
# literally named "count" to load the file at all, and facetedTable.c keys its
# merge logic on "count", "color" and "val". Renaming them breaks the track.
# Timepoint values, colours and row order are also untouched; row order matters
# because the bigBed expScores are positional.
#
# Renaming the tissue/timepoint columns means trackDb must match, since facets
# are resolved by column name:  barChartFacets Tissue,Timepoint
#
# The hub at woldlab.caltech.edu still ships lower-case, so like the tissue
# reorder and the colour update this has to be reapplied after any refetch.
# Idempotent, so re-running is safe. See #37001.
#
# Usage: sentenceCaseTissues.sh <file.facets|file.categories> ...
# Originals are kept alongside as <name>.preCase

set -euo pipefail

[ $# -ge 1 ] || { echo "usage: sentenceCaseTissues.sh <file> ..." >&2; exit 1; }

for F in "$@"; do
    [ -s "$F" ] || { echo "missing: $F" >&2; exit 1; }
    NAME=$(basename "$F")
    case "$NAME" in
        *.facets)     KIND=facets ;;
        *.categories) KIND=categories ;;
        *) echo "$NAME is neither .facets nor .categories" >&2; exit 1 ;;
    esac

    awk -F'\t' -v OFS='\t' -v k="$KIND" '
        function cap(s) { return toupper(substr(s,1,1)) substr(s,2) }
        NR==1 && k=="facets" {
            # Facet column titles are rendered verbatim, so capitalise the two
            # faceted columns. Leave count and color alone: matched by exact
            # lower-case string in the C code.
            for (i=1; i<=NF; i++)
                if ($i=="tissue" || $i=="timepoint") $i = cap($i)
            print; next
        }
        NF==0 { print; next }
        {
            $1 = cap($1)                                  # label
            if (k=="facets" && NF>=3) $3 = cap($3)         # tissue value
            print
        }' "$F" > "$F.tmp"

    before=$(wc -l < "$F"); after=$(wc -l < "$F.tmp")
    [ "$before" = "$after" ] || { echo "row count changed $before -> $after" >&2; rm -f "$F.tmp"; exit 1; }

    if cmp -s "$F" "$F.tmp"; then
        rm -f "$F.tmp"; echo "  $NAME already sentence-cased, unchanged"
    else
        [ -e "$F.preCase" ] || cp -p "$F" "$F.preCase"
        mv "$F.tmp" "$F"; chmod 664 "$F"
        echo "  $NAME: $after rows updated"
    fi
done
