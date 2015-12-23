include ../../../../inc/common.mk

chainToPsl=${DESTBINDIR}/chainToPsl
pslCheck=${DESTBINDIR}/pslCheck


all::

test: example1 example2

example1: mkdirs
	@${chainToPsl} input/$@.chain /dev/null /dev/null \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit output/$@.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
		-querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.psl
	@diff expected/$@.psl output/$@.psl

example2: mkdirs
	@${chainToPsl} input/$@.chain /dev/null /dev/null \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit output/$@.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
		-querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.psl
	@diff expected/$@.psl output/$@.psl

mkdirs:
	@${MKDIR} output
	@twoBitInfo ../../lavToAxt/tests/input/hg19.chrM.2bit \
		output/hg19.chrM.sizes
	@twoBitInfo ../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/susScr3.chrM.sizes

clean::
	@rm -rf output
