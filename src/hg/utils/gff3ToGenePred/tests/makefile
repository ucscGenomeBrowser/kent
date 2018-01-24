kentSrc = ../../../..
include ../../../../inc/common.mk

gff3ToGenePred = ${DESTBINDIR}/gff3ToGenePred

# sh commands to deal with gff3ToGenePred shouldhave failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all::

test:: geneMRnaTest noGeneMRnaTest geneMRnaHonorTest discontinuousTest multCdsOutOfExonTest \
	noIdTest errCases1Test bogusQuotesTest noExonsTest geneTranscriptTest transcriptCdsParentTest \
	minimalGenesTest geneDefaultStatusUnknownTest useNameTest nameAttrIdTest nameAttrNameTest \
	frameShiftTest mm10GencodeTest ncbiSegmentsTest

geneMRnaTest: mkout
	${gff3ToGenePred} input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

noGeneMRnaTest: mkout
	${gff3ToGenePred} input/noGeneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

geneMRnaHonorTest: mkout
	${gff3ToGenePred} -honorStartStopCodons input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

geneDefaultStatusUnknownTest: mkout
	${gff3ToGenePred} -defaultCdsStatusToUnknown input/geneMRna.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

discontinuousTest: mkout
	${gff3ToGenePred} input/discontinuous.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# was only reporting the first error
multCdsOutOfExonTest: mkout
	if ! ${gff3ToGenePred} input/multCdsOutOfExon.gff3 /dev/null >output/$@.out 2>&1 ; then true ; else ${cmdShouldFail} ; fi
	diff expected/$@.out output/$@.out

# some records without ID or Parent attrs
noIdTest: mkout
	${gff3ToGenePred} input/noId.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# error cases that should be handled
errCases1Test: mkout
	if ! ${gff3ToGenePred} input/errCases1.gff3 /dev/null >output/$@.out 2>&1 ; then true ; else ${cmdShouldFail} ; fi
	diff expected/$@.out output/$@.out

# error cases involving quotes
bogusQuotesTest: mkout
	if ! ${gff3ToGenePred} input/bogusQuotes.gff3 /dev/null >output/$@.out 2>&1 ; then true ; else ${cmdShouldFail} ; fi
	diff expected/$@.out output/$@.out

noExonsTest: mkout
	${gff3ToGenePred} input/noExons.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# from PlasmoDB: ncRNAs have gene->transcript->exon
geneTranscriptTest: mkout
	${gff3ToGenePred} input/geneTranscript.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# it also appears that gene->transcript->exon->cds is valid
transcriptCdsParentTest: mkout
	${gff3ToGenePred} input/transcriptCdsParent.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# pseudogene annotations tie exons directly to genes and have standalong gnenes
minimalGenesTest: mkout
	${gff3ToGenePred} -allowMinimalGenes -unprocessedRootsOut=output/$@.unprocessed input/ncbiRefSeq.pseudoGenes.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.unprocessed output/$@.unprocessed

# set name/name2 using -useName
useNameTest: mkout
	${gff3ToGenePred} -attrsOut=output/$@.attrs -useName input/transcriptCdsParent.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.attrs output/$@.attrs

nameAttrIdTest: mkout
	${gff3ToGenePred} -rnaNameAttr=transcript_id -geneNameAttr=gene_id input/transcriptCdsParent.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

nameAttrNameTest: mkout
	${gff3ToGenePred} -attrsOut=output/$@.attrs -rnaNameAttr=transcript_name -geneNameAttr=gene_name input/transcriptCdsParent.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.attrs output/$@.attrs

frameShiftTest: mkout
	${gff3ToGenePred} input/frameShifts.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp

# gencode conversion
mm10GencodeTest: mkout
	${gff3ToGenePred} -rnaNameAttr=transcript_id -geneNameAttr=gene_id -honorStartStopCodons -attrsOut=output/$@.attrs input/mm10Gencode.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.attrs output/$@.attrs

# NCBI [CDJV]_gene_segment annotations
ncbiSegmentsTest: mkout
	${gff3ToGenePred} -rnaNameAttr=transcript_id -geneNameAttr=gene -honorStartStopCodons -attrsOut=output/$@.attrs input/mm10Gencode.gff3 output/$@.gp
	diff expected/$@.gp output/$@.gp
	diff expected/$@.attrs output/$@.attrs

mkout:
	@mkdir -p output

clean::
	rm -rf output
