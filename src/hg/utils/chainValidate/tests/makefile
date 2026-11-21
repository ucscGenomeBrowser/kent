kentSrc = ../../../..
include ../../../../inc/common.mk

chainValidate = ${DESTBINDIR}/chainValidate

all:: test

test:: goodTest badTest sizesTest

# A clean file: no output and exit status 0.
goodTest: mkout
	${chainValidate} input/good.chain > output/$@.out
	diff expected/$@.out output/$@.out

# One chain for each problem, including chains that should be joined on each strand.
# The exit status must be 1.
badTest: mkout
	if ${chainValidate} input/bad.chain > output/$@.out; then exit 1; fi
	diff expected/$@.out output/$@.out

# Sizes that differ from a chrom.sizes file, and a sequence missing from one.
sizesTest: mkout
	if ${chainValidate} -tSizes=input/t.sizes -qSizes=input/q.sizes input/good.chain \
	    > output/$@.out; then exit 1; fi
	diff expected/$@.out output/$@.out

mkout:
	@mkdir -p output

clean::
	rm -rf output
