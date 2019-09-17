kentSrc = ../../../..
include ../../../../inc/common.mk

chainToPsl=${DESTBINDIR}/chainToPsl
pslCheck=${DESTBINDIR}/pslCheck


all::

test: example1 example2
	${MAKE} clean

example1: mkdirs
	@${chainToPsl} input/$@.chain.gz /dev/null /dev/null \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit output/$@.psl > /dev/null 2>&1
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
		-querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.psl
	@rm -f output/$@.psl.gz
	@gzip output/$@.psl
	@zdiff expected/$@.psl.gz output/$@.psl.gz

example2: mkdirs
	@${chainToPsl} input/$@.chain.gz /dev/null /dev/null \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit output/$@.psl > /dev/null 2>&1
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
		-querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.psl
	@rm -f output/$@.psl.gz
	@gzip output/$@.psl
	@zdiff expected/$@.psl.gz output/$@.psl.gz

mkdirs:
	@${MKDIR} output
	@twoBitInfo ../../lavToAxt/tests/input/hg19.chrM.2bit \
		output/hg19.chrM.sizes
	@twoBitInfo ../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/susScr3.chrM.sizes

clean::
	@rm -rf output
