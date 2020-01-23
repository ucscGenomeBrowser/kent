#!/bin/bash

# the taxonomy/2020-01-05.vertData listing is made in that
# directory with the script:
# /hive/data/outside/ncbi/genomes/taxId/taxonomy/updateLists.sh
#

export clade="mammals"

awk -F$'\t' 'BEGIN{processing=0}{
  if (match($0,"primates")) { processing = 1; }
  if (match($0,"mammals")) { processing = 0; } else {
    if (! match($0,"^#")) {
      if (processing) { print $0; }
    }
  }
}' /hive/data/outside/ncbi/genomes/taxId/taxonomy/2020-01-05.vertData \
  | cut -f2 > before.cleaning

awk -F$'\t' 'BEGIN{processing=0}{
  if (match($0,"primates")) { processing = 1; }
  if (match($0,"mammals")) { processing = 0; } else {
    if (! match($0,"^#")) {
      if (processing) { print $0; }
    }
  }
}' /hive/data/outside/ncbi/genomes/taxId/taxonomy/2020-01-05.vertData \
  | cut -f2 | sed -e 's/10042/230844/; s/189058/1706337/; s/34882/391180/; /59538/d; /89399/d; s/\b9612\b/9615/; s/9644/116960/; s/9668/9669/; s/9694/74533/; s/9707/9708/; s/9767/310752/; s/9778/127582/; s/9807/73337/; s/9818/1230840/; s/9874/9880/; s/9901/43346/; s/9993/9994/;' | sort > $clade.taxId.list


join -t$'\t' $clade.taxId.list \
  <(sort /hive/data/outside/ncbi/genomes/refseq/listingVerts/xref.names.2020-01-05.tsv) \
    > $clade.xrefName.list

awk -F$'\t' '{printf "%s\t%s\n", $13, $3}' $clade.xrefName.list \
  | sort > ../asmHubs/$clade.asmId.commonName.tsv

awk -F$'\t' '{printf "%s\t%s\n", $3, $13}' $clade.xrefName.list \
  | sort -fr > ../asmHubs/$clade.commonName.asmId.orderList.tsv
