#!/bin/bash

set -beEu -o pipefail

hgsql -N -hgenome-centdb -e 'select name from dbDb where active=1;' hgcentral \
   | sort > rr.active.list

hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE
  sourceAuthority="ucsc" AND destinationAuthority="refseq" AND
 matchCount=sourceCount;' hgFixed | sort -u > ucsc.refseq.list

join -t$'\t' rr.active.list ucsc.refseq.list > rr.refseq.equiv.list
join -v1 -t$'\t' rr.active.list ucsc.refseq.list > rr.not.refseq.list

hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE
  sourceAuthority="ucsc" AND destinationAuthority="genbank" AND
 matchCount=sourceCount;' hgFixed | sort -u > ucsc.check.genbank.list

join -t$'\t' rr.not.refseq.list ucsc.check.genbank.list > ucsc.genbank.list
join -t$'\t' rr.active.list ucsc.genbank.list > rr.genbank.equiv.list

cut -f1 ucsc.refseq.list ucsc.genbank.list | sort -u \
   | join -v1 rr.active.list - > ucsc.rr.not.perfect

hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE
  sourceAuthority="ucsc" AND destinationAuthority="refseq" AND
 1=ABS(matchCount-sourceCount);' hgFixed | sort -u \
   | join -t$'\t' ucsc.rr.not.perfect - > ucsc.rr.refseq.oneMismatch.list

hgsql -N -e 'SELECT source,destination FROM asmEquivalent WHERE
  sourceAuthority="ucsc" AND destinationAuthority="genbank" AND
 1=ABS(matchCount-sourceCount);' hgFixed | sort -u \
   | join -t$'\t' ucsc.rr.not.perfect - > ucsc.check.genbank.oneMismatch.list

cut -f1 ucsc.rr.refseq.oneMismatch.list \
   | join -t$'\t' -v2 - ucsc.check.genbank.oneMismatch.list \
      > ucsc.rr.genbank.oneMismatch.list

sort rr.refseq.equiv.list rr.genbank.equiv.list \
  ucsc.rr.refseq.oneMismatch.list ucsc.rr.genbank.oneMismatch.list \
     > rr.GCF.GCA.list

cut -f1 rr.refseq.equiv.list rr.genbank.equiv.list \
  ucsc.rr.refseq.oneMismatch.list ucsc.rr.genbank.oneMismatch.list \
    | sort > rr.matched
