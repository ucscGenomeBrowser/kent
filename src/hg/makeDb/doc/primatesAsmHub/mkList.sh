#!/bin/bash

# the taxonomy/2019-12-12.vertData listing is made in that
# directory with the script:
# /hive/data/outside/ncbi/genomes/taxId/taxonomy/updateLists.sh
#

awk -F$'\t' 'BEGIN{processing=1}{
  if (match($0,"primates")) { processing = 0; } else {
    if (! match($0,"^#")) {
      if (processing) { print $0; }
    }
  }
}' /hive/data/outside/ncbi/genomes/taxId/taxonomy/2019-12-12.vertData \
  | cut -f2 | sort > primates.taxId.list

join -t$'\t' primates.taxId.list \
  <(sort /hive/data/outside/ncbi/genomes/refseq/listingVerts/xref.names.2019-12-05.tsv) \
    > primates.xrefName.list

awk -F$'\t' '{printf "%s\t%s\n", $13, $3}' primates.xrefName.list \
  | sort > ../asmHubs/primates.asmId.commonName.tsv

awk -F$'\t' '{printf "%s\t%s\n", $3, $13}' primates.xrefName.list \
  | sort -fr > ../asmHubs/primates.commonName.asmId.orderList.tsv
