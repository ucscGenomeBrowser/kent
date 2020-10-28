kentSrc = ../../..
A = ${DESTBINDIR}/pslRemoveFrameShifts 
include ../../../inc/common.mk

include ../../../inc/common.mk


test:	outdir positive negative

outdir:
	mkdir -p out

positive:
	${A} input/positive.psl out/positive.psl
	diff expected/positive.psl out/positive.psl

negative:
	${A} input/negative.psl out/negative.psl
	diff expected/negative.psl out/negative.psl

clean:
	rm -rf out
