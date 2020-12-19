kentSrc = ../../../..
include ../../../../inc/common.mk

transMapPslToGenePred = ${DESTBINDIR}/transMapPslToGenePred

diff = diff -u

all:

test: musTest nonCodingFillTest codingFillTest fillTest fill30Test \
	cds2InsertTest tmAstMex1Tests

musTest: mkdirs
	${transMapPslToGenePred} -frameShifts=output/$@.fs.tsv input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv

nonCodingFillTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=10 -frameShifts=output/$@.fs.tsv input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv

codingFillTest: mkdirs
	${transMapPslToGenePred} -codingGapFillMax=13 -frameShifts=output/$@.fs.tsv input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv

fillTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=20 -codingGapFillMax=10 -frameShifts=output/$@.fs.tsv input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv

fill30Test: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=30 -codingGapFillMax=30 -frameShifts=output/$@.fs.tsv input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv

# ENST00000625058.1 - two base genomic insertion in CDS
# ENST00000486585.1 - incomplete CDS start and maps to opposite strand
cds2InsertTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=30 -codingGapFillMax=30 -frameShifts=output/$@.fs.tsv input/chimp.src.gp input/chimp.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv

# test cases from transmap V5 astMex1 to hg38
tmAstMex1Tests: tmAstMex1NoMergeTests

tmAstMex1NoMergeTests: mkdirs
	${transMapPslToGenePred} -noBlockMerge -frameShifts=output/$@.fs.tsv input/tm-astMex1.ensembl.gp input/tm-astMex1-hg38.ensembl.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp
	${diff} expected/$@.fs.tsv output/$@.fs.tsv


clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
