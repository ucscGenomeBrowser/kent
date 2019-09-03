kentSrc = ../../../../..
include ${kentSrc}/inc/common.mk

diff = diff -u

gencodeGxfToAttrs = ../bin/gencodeGxfToAttrs


# set1 includes PAR gene with renaming on chrY ENSG00000182484 -> ENSGR0000182484

test:: set1GtfTest v21GtfTest v21Gff3Test v27ParGtfTest v27ParGff3Test v19GtfTest

set1GtfTest: mkdirs
	${gencodeGxfToAttrs} input/set1.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

v21GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v21.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

v21Gff3Test: mkdirs
	${gencodeGxfToAttrs} input/v21.gff3 output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

v27ParGtfTest: mkdirs
	${gencodeGxfToAttrs} input/v27.par.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

v27ParGff3Test:mkdirs
	${gencodeGxfToAttrs} input/v27.par.gff3 output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

# compatiblity with V19 for reloading
v19GtfTest: mkdirs
	${gencodeGxfToAttrs} input/v19.gtf output/$@.attrs output/$@.tsl
	${diff} expected/$@.attrs output/$@.attrs
	${diff} expected/$@.tsl output/$@.tsl

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
