kentSrc = ../../../..
include ../../../../inc/common.mk

rmskAlignToPsl = ${DESTBINDIR}/rmskAlignToPsl

# input/hg38.fa.align.tsv is from Dfam_2.0

pslCheck = pslCheck -verbose=0

all::

test:: hg38Test mm39Test mm39ProbTest hg38BigRmskTest wrongSizeTest

hg38Test: mkout
	${rmskAlignToPsl} input/hg38.align.tsv output/$@.psl
	diff expected/$@.psl output/$@.psl

mm39Test: mkout
	${rmskAlignToPsl} input/mm39.align.tsv output/$@.psl
	diff expected/$@.psl output/$@.psl

mm39ProbTest: mkout
	${rmskAlignToPsl} input/mm39.prob.align.tsv output/$@.psl >output/$@.err 2>&1
	diff expected/$@.psl output/$@.psl
	diff expected/$@.err output/$@.err
	${pslCheck} output/$@.psl

hg38BigRmskTest: mkout
	${rmskAlignToPsl} -bigRmsk input/hg38.dfam_3.3.bigRmskIn output/$@.psl
	diff expected/$@.psl output/$@.psl
	${pslCheck} output/$@.psl

wrongSizeTest: mkout
	${rmskAlignToPsl} -repSizes=input/wrongSize.sizes -bigRmsk input/wrongSize.bigRmskIn output/$@.psl 2> output/$@.out
	diff expected/$@.psl output/$@.psl
	diff expected/$@.out output/$@.out
	${pslCheck} output/$@.psl

mkout:
	@mkdir -p output

clean::
	rm -rf output
