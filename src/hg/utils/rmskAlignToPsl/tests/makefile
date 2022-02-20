kentSrc = ../../../..
include ../../../../inc/common.mk

rmskAlignToPsl = ${DESTBINDIR}/rmskAlignToPsl

# input/hg38.fa.align.tsv is from Dfam_2.0


all::

test:: hg38Test mm39Test  mm39ProbTest

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

mkout:
	@mkdir -p output

clean::
	rm -rf output
