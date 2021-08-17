#!/bin/bash

set -beEu -o pipefail

# Entrez search for all SARS-CoV-2 sequences with length >= 29,000
# "Severe acute respiratory syndrome coronavirus 2"[Organism] AND ("29000"[SLEN] : "35000"[SLEN])
query='%22Severe%20acute%20respiratory%20syndrome%20coronavirus%202%22%5BOrganism%5D'

tool=searchAllSarsCov2.sh
email="$USER%40soe.ucsc.edu"

# Assemble the esearch URL
base="https://eutils.ncbi.nlm.nih.gov/entrez/eutils/"
url="${base}esearch.fcgi?db=biosample&term=$query&tool=$tool&email=$email&retmax=10000000"

curl -s -S "$url" \
| grep "<Id>" \
| sed -re 's@\s*<Id>([0-9]+)</Id>@\1@;' \
    > all.biosample.gids.txt
