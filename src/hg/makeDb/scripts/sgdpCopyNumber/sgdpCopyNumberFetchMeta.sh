#!/bin/bash
# Fetch the Simons Genome Diversity Project sample tables used to build the facets
# for the hs1 sgdpCopyNumber track.  refs #29344
#
# The copy-number bigBeds themselves came from the Eichler lab in 2022 and are not
# re-fetched here; only the sample metadata is.
#
# Writes into /hive/data/genomes/hs1/bed/sgdpCopyNumber/metaSrc.

set -beEu -o pipefail

destDir=${1:-/hive/data/genomes/hs1/bed/sgdpCopyNumber/metaSrc}
base=https://sharehost.hms.harvard.edu/genetics/reich_lab/sgdp

mkdir -p "$destDir"

# 344 samples: 279 fully public + 21 signed-letter + 44 from Fan et al.
# Carries Region, Country, Town, Population_ID, Gender, DNA_Source, lat/long,
# keyed on the sequencing library id (Illumina_ID), which is what the bigBed
# file names use.
#
# 280 samples with an ENA/BioSample accession per library id.  Used only to add
# a BioSample link to the table; it covers fewer samples than the file above.
for f in SGDP_metadata.279public.21signedLetter.44Fan.samples.txt ena.ftp.pointers.txt ; do
    # Download to .part and move into place, so an interrupted fetch can never
    # leave a half-written file that looks complete.
    curl -sSL --fail -o "$destDir/$f.part" "$base/$f"
    mv "$destDir/$f.part" "$destDir/$f"
    printf "%s: %d lines\n" "$f" "$(wc -l < "$destDir/$f")"
done
