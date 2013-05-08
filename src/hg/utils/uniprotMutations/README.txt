This is the pipeline to convert uniProt mutations or annoations to a browser track.

It relies on the publications track uniprot parser, which is not part of this
directory, but should be in /hive/data/inside/pubs/bin or on github as pubMunch.
The script is called pubParseDb and is run as "pubParseDb uniprot". It converts
uniprot to tab-separated files, different from the ones that Jim's sp parser
creates, it parses the variants and evidence/reference information.

the two main steps to get uniprot mutations to the genome are:
1) makeUniProtToHg.sh - create a psl file to lift protein coordinates to
genome coordinates. This script is based on markd's ls-snp pipeline.
It creates uniProtToHg19.psl

2) liftUniprotMut.py - a python script to parse the mutation tab file 
from the uniprot parser to bed, lift the bed with pslMap, then annotate
the result with the original fields from the mutation tab file. It creates
uniprotMutations.hg19.bed.

liftUniprotMut.py also accepts the parameter --annot: this will create a track 
not for mutations, but for all other variants (actually, not all, just the 
not-software-predicted ones).

Two other pipelines can be found on the internet that map uniprot variants to
the genome: one from the uniprot people themselves, for ensembl and one by 
belinda giardine, for phencode.  The main difference is that I'm mapping only
disease relevant mutations and parse a lot more information directly into the
tables. They get a higher number of mutations, but many are not annotated 
with evidence, references or any other comments. That said, there might be 
more bugs in my script, as I do a lot more parsing.

Max
