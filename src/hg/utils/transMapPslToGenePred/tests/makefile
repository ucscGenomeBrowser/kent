kentSrc = ../../../..
include ../../../../inc/common.mk

transMapPslToGenePred = ${DESTBINDIR}/transMapPslToGenePred

diff = diff -u

all:

test: musTest nonCodingFillTest codingFillTest fillTest fill30Test \
	cds2InsertTest
	${MAKE} clean

musTest: mkdirs
	${transMapPslToGenePred} input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp

nonCodingFillTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=10 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp

codingFillTest: mkdirs
	${transMapPslToGenePred} -codingGapFillMax=13 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp

fillTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=20 -codingGapFillMax=10 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp

fill30Test: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=30 -codingGapFillMax=30 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp

# ENST00000625058.1 - two base genomic insertion in CDS
# ENST00000486585.1 - incomplete CDS start and maps to opposite strand
cds2InsertTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=30 -codingGapFillMax=30 input/chimp.src.gp input/chimp.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	${diff} expected/$@.mapped.gp output/$@.mapped.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
