#!/bin/bash

set -beEu -o pipefail

function runList() {
  sh nearMiss.${1}.${2}.run.list > results/match.${1}.${2}.txt \
      2> results/notMatch.${1}.${2}.txt
}

mkdir -p results

~/kent/src/hg/makeDb/doc/assemblyEquivalence/A.vs.B.sh ensembl genbank
~/kent/src/hg/makeDb/doc/assemblyEquivalence/A.vs.B.sh ensembl refseq
~/kent/src/hg/makeDb/doc/assemblyEquivalence/A.vs.B.sh ensembl ucsc
~/kent/src/hg/makeDb/doc/assemblyEquivalence/A.vs.B.sh ucsc genbank
~/kent/src/hg/makeDb/doc/assemblyEquivalence/A.vs.B.sh ucsc refseq
~/kent/src/hg/makeDb/doc/assemblyEquivalence/A.vs.B.sh genbank refseq

### ensembl v. others
runList ensembl refseq &
runList refseq ensembl &
runList ensembl ucsc &
runList ucsc ensembl &
runList ensembl genbank &
runList genbank ensembl &

### ucsc v. others
runList refseq ucsc &
runList ucsc refseq &
runList ucsc genbank &
runList genbank ucsc &

### finally genbank v. refseq
runList genbank refseq &
runList refseq genbank

wait
