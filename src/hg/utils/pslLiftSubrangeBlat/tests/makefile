kentSrc = ../../..
include ../../../inc/common.mk

pslLiftSubrangeBlat=${DESTBINDIR}/pslLiftSubrangeBlat
pslCheck=${DESTBINDIR}/pslCheck

all:

test: tSubRangeTest qSubRangeTest qtSubRangeTest

tSubRangeTest: mkdirs
	${pslLiftSubrangeBlat} -tSizes=input/tSubrange.tsizes input/tSubrange.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff expected/$@.psl output/$@.psl

qSubRangeTest: mkdirs
	${pslLiftSubrangeBlat} -qSizes=input/tSubrange.tsizes input/qSubrange.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff expected/$@.psl output/$@.psl

qtSubRangeTest: mkdirs
	${pslLiftSubrangeBlat} -qSizes=input/qtSubrange.sizes -tSizes=input/qtSubrange.sizes input/qtSubrange.psl output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff expected/$@.psl output/$@.psl

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output

