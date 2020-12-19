#!/bin/bash

# the taxonomy/2020-01-05.vertData listing is made in that
# directory with the script:
# /hive/data/outside/ncbi/genomes/taxId/taxonomy/updateLists.sh
#

export clade="birds"

awk -F$'\t' 'BEGIN{processing=0}{
  if (match($0,"\tmammals\t")) { processing = 1; }
  if (match($0,"aves")) { processing = 0; } else {
    if (! match($0,"^#")) {
      if (processing) { print $0; }
    }
  }
}' /hive/data/outside/ncbi/genomes/taxId/taxonomy/2020-01-05.vertData \
  |  cut -f2 | sort > $clade.taxId.list

join -t$'\t' $clade.taxId.list \
  <(sort /hive/data/outside/ncbi/genomes/refseq/listingVerts/xref.names.2020-01-05.tsv) \
    > $clade.xrefName.list

awk -F$'\t' '{printf "%s\t%s\n", $13, $3}' $clade.xrefName.list \
  | sort > ../asmHubs/$clade.asmId.commonName.tsv

awk -F$'\t' '{printf "%s\t%s\n", $3, $13}' $clade.xrefName.list \
  | sort -fr > ../asmHubs/$clade.commonName.asmId.orderList.tsv
