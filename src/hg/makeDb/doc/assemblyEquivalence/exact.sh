#!/bin/bash

onePair() {
join -t$'\t' $1 $2 | cut -f2- | sort
}

#### Ensembl to the other three
onePair ensembl/ensembl.keySignatures.txt ucsc/ucsc.keySignatures.txt \
   > ensembl.ucsc.exact.txt
onePair ucsc/ucsc.keySignatures.txt ensembl/ensembl.keySignatures.txt \
   > ucsc.ensembl.exact.txt

onePair ensembl/ensembl.keySignatures.txt refseq/refseq.keySignatures.txt \
   > ensembl.refseq.exact.txt
onePair refseq/refseq.keySignatures.txt ensembl/ensembl.keySignatures.txt \
   > refseq.ensembl.exact.txt

onePair ensembl/ensembl.keySignatures.txt genbank/genbank.keySignatures.txt \
   > ensembl.genbank.exact.txt
onePair genbank/genbank.keySignatures.txt ensembl/ensembl.keySignatures.txt \
   > genbank.ensembl.exact.txt

#### ucsc to the other two
onePair ucsc/ucsc.keySignatures.txt refseq/refseq.keySignatures.txt \
   > ucsc.refseq.exact.txt
onePair refseq/refseq.keySignatures.txt ucsc/ucsc.keySignatures.txt \
   > refseq.ucsc.exact.txt

onePair ucsc/ucsc.keySignatures.txt genbank/genbank.keySignatures.txt \
   > ucsc.genbank.exact.txt
onePair genbank/genbank.keySignatures.txt ucsc/ucsc.keySignatures.txt \
   > genbank.ucsc.exact.txt

#### genbank to refseq

onePair refseq/refseq.keySignatures.txt genbank/genbank.keySignatures.txt \
   > refseq.genbank.exact.txt
onePair genbank/genbank.keySignatures.txt refseq/refseq.keySignatures.txt \
   > genbank.refseq.exact.txt
