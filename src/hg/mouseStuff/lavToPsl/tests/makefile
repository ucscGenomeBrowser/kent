kentSrc = ../../../..
include ../../../../inc/common.mk

lavToPsl=${DESTBINDIR}/lavToPsl
lavToAxt=${DESTBINDIR}/lavToAxt
axtToPsl=${DESTBINDIR}/axtToPsl
pslCheck=${DESTBINDIR}/pslCheck
pslRecalcMatch=${DESTBINDIR}/pslRecalcMatch

all::

test: oldStyleBlastz newStyleLastz redmine12502 trimEndsBug


newStyleLastz:	mkdirs
	@${lavToPsl} input/$@.lav output/$@.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
		-querySizes=output/susScr3.chrM.sizes -verbose=0 output/$@.psl
	@diff expected/$@.psl output/$@.psl
	@${pslRecalcMatch} output/$@.psl \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/$@.recalc0.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
                -querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.recalc0.psl
	@${lavToAxt} -scoreScheme=../../lavToAxt/tests/input/$@.Q.txt \
		input/$@.lav ../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit output/$@.axt
	@${axtToPsl} output/$@.axt output/hg19.chrM.sizes \
		output/susScr3.chrM.sizes output/$@.axt.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
                -querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.axt.psl
	@${pslRecalcMatch} output/$@.axt.psl \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/$@.recalc1.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
                -querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.recalc1.psl
	@diff output/$@.recalc0.psl output/$@.recalc1.psl
	@${lavToPsl} -bed input/$@.lav output/$@.bed
	@diff expected/$@.bed output/$@.bed

oldStyleBlastz:	mkdirs
	@${lavToPsl} input/$@.lav output/$@.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
		-querySizes=output/susScr3.chrM.sizes -verbose=0 output/$@.psl
	@diff expected/$@.psl output/$@.psl
	@${pslRecalcMatch} output/$@.psl \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/$@.recalc0.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
                -querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.recalc0.psl
	@${lavToAxt} -scoreScheme=../../lavToAxt/tests/input/$@.Q.txt \
		input/$@.lav ../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit output/$@.axt
	@${axtToPsl} output/$@.axt output/hg19.chrM.sizes \
		output/susScr3.chrM.sizes output/$@.axt.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
                -querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.axt.psl
	@${pslRecalcMatch} output/$@.axt.psl \
		../../lavToAxt/tests/input/hg19.chrM.2bit \
		../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/$@.recalc1.psl
	@${pslCheck} -targetSizes=output/hg19.chrM.sizes \
                -querySizes=output/susScr3.chrM.sizes -verbose=0 \
		output/$@.recalc1.psl
	@diff output/$@.recalc0.psl output/$@.recalc1.psl
	@${lavToPsl} -bed input/$@.lav output/$@.bed
	@diff expected/$@.bed output/$@.bed

redmine12502: mkdirs
	@${lavToPsl} input/$@.lav.gz output/$@.psl
	@${pslCheck} -targetSizes=input/hg19.chrom.sizes.gz \
		-querySizes=input/mm10.chrom.sizes.gz -verbose=0 output/$@.psl
	@zdiff expected/$@.psl.gz output/$@.psl
	@${lavToPsl} -bed input/$@.lav.gz output/$@.bed
	@zdiff expected/$@.bed.gz output/$@.bed

trimEndsBug: mkdirs
	@${lavToPsl} input/$@.lav output/$@.psl
	@${pslCheck} -verbose=0 output/$@.psl

mkdirs:
	@${MKDIR} output
	@twoBitInfo ../../lavToAxt/tests/input/hg19.chrM.2bit \
		output/hg19.chrM.sizes
	@twoBitInfo ../../lavToAxt/tests/input/susScr3.chrM.2bit \
		output/susScr3.chrM.sizes

clean::
	@rm -fr output

