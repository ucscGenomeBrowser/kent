#!/bin/bash
# Download the N=1 Collaborative (N1C) Gene Registry tables from its data API
# (documented at https://generegistry.n1collaborative.org/api) into the current
# directory, one JSON file per table.
# The API builds the JSON on the fly, so there is no size to compare with. Each
# table goes to <table>.json.part first and is only moved into place when it
# parses as a non-empty JSON list.
# usage: n1cRegistryDownload.sh [table ...]   (default: the three tables we use)
set -beEu -o pipefail

base="https://gene-registry.onrender.com/api/data?table="
tables=${@:-"assessed_variants N1C_projects marketed_drugs"}

for t in $tables; do
    # the API runs on a free hosting plan and can take a minute to wake up
    curl -sS --fail --retry 3 --retry-delay 20 -m 300 -o $t.json.part "$base$t"
    n=$(python3 -I -c 'import json,sys; d=json.load(open(sys.argv[1])); assert isinstance(d, list) and d, "not a non-empty list"; print(len(d))' $t.json.part)
    mv $t.json.part $t.json
    echo "$t: $n rows"
done
