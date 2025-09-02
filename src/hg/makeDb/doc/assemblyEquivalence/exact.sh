#!/bin/bash

onePair() {
join -t$'\t' $1 $2 | cut -f2- | sort
}

#### Ensembl rapidRelease to the other four
# for T in ensembl ucsc refseq genbank
for T in ensembl ucsc refseq genbank
do
onePair rapidRelease/rapidRelease.keySignatures.txt \
    $T/$T.keySignatures.txt > rapidRelease.$T.exact.txt
onePair $T/$T.keySignatures.txt \
    rapidRelease/rapidRelease.keySignatures.txt > $T.rapidRelease.exact.txt
done

#### Ensembl to the other three
# for T in ucsc refseq genbank
for T in ucsc refseq genbank
do
onePair ensembl/ensembl.keySignatures.txt $T/$T.keySignatures.txt \
   > ensembl.$T.exact.txt
onePair $T/$T.keySignatures.txt ensembl/ensembl.keySignatures.txt \
   > $T.ensembl.exact.txt
done

#### ucsc to the other two
# for T in refseq genbank
for T in refseq genbank
do
onePair ucsc/ucsc.keySignatures.txt $T/$T.keySignatures.txt \
   > ucsc.$T.exact.txt
onePair $T/$T.keySignatures.txt ucsc/ucsc.keySignatures.txt \
   > $T.ucsc.exact.txt
done

#### genbank to refseq

onePair refseq/refseq.keySignatures.txt genbank/genbank.keySignatures.txt \
   > refseq.genbank.exact.txt
onePair genbank/genbank.keySignatures.txt refseq/refseq.keySignatures.txt \
   > genbank.refseq.exact.txt
