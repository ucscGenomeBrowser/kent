kentSrc = ../../../..
include ../../../../inc/common.mk

# will have to update results from time to time due to database updates
db = hg18

refSeqGet = ${DESTBINDIR}/refSeqGet
outExts = psl gp prot.fa rna.fa meta

test:: basicListTest  versionedListTest

basicListTest: mkout
	${refSeqGet} -aligns=output/$@.psl -geneAnns=output/$@.gp -rnaSeqs=output/$@.rna.fa -protSeqs=output/$@.prot.fa -accList=input/basic.acc -metaData=output/$@.meta ${db}
	${MAKE} doDiffs id=$@

versionedListTest: mkout
	${refSeqGet} -aligns=output/$@.psl -geneAnns=output/$@.gp -rnaSeqs=output/$@.rna.fa -protSeqs=output/$@.prot.fa -accList=input/versioned.acc -metaData=output/$@.meta ${db}
	${MAKE} doDiffs id=$@

# recursive target to do diffs for ${id}
doDiffs: ${outExts:%=%.doDiff}

%.doDiff:
	diff -u expected/${id}.$* output/${id}.$*

# big tests to run by hand, no expected results to diff
bigTest: bigTestNM bigTestNR bigTestBoth

bigTestNM: mkout
	${refSeqGet} -getNM -aligns=output/$@.psl -geneAnns=output/$@.gp -rnaSeqs=output/$@.rna.fa -protSeqs=output/$@.prot.fa -metaData=output/$@.meta ${db}

bigTestNR: mkout
	${refSeqGet} -getNR -aligns=output/$@.psl -geneAnns=output/$@.gp -rnaSeqs=output/$@.rna.fa -protSeqs=output/$@.prot.fa -metaData=output/$@.meta ${db}

bigTestBoth: mkout
	${refSeqGet} -aligns=output/$@.psl -geneAnns=output/$@.gp -rnaSeqs=output/$@.rna.fa -protSeqs=output/$@.prot.fa -metaData=output/$@.meta ${db}

mkout::
	@mkdir -p output
clean::
	rm -rf output

