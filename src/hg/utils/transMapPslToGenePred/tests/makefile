kentSrc = ../../../..
include ../../../../inc/common.mk

transMapPslToGenePred = ${DESTBINDIR}/transMapPslToGenePred

all:

test: musTest nonCodingFillTest codingFillTest fillTest fillTest30

musTest: mkdirs
	${transMapPslToGenePred} input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	diff expected/$@.mapped.gp output/$@.mapped.gp

nonCodingFillTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=10 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	diff expected/$@.mapped.gp output/$@.mapped.gp

codingFillTest: mkdirs
	${transMapPslToGenePred} -codingGapFillMax=13 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	diff expected/$@.mapped.gp output/$@.mapped.gp

fillTest: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=20 -codingGapFillMax=10 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	diff expected/$@.mapped.gp output/$@.mapped.gp

fillTest30: mkdirs
	${transMapPslToGenePred} -nonCodingGapFillMax=30 -codingGapFillMax=30 input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	genePredCheck -verbose=0 output/$@.mapped.gp
	diff expected/$@.mapped.gp output/$@.mapped.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
