include ../../../../inc/common.mk

transMapPslToGenePred = ${DESTBINDIR}/transMapPslToGenePred

all:

test: musTest

musTest: mkdirs
	${transMapPslToGenePred} input/musstrain.src.gp input/musstrain.mapped.psl output/$@.mapped.gp
	diff expected/$@.mapped.gp output/$@.mapped.gp

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
