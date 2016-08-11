kentSrc = ../../../..
include ../../../../inc/common.mk

gff3ToPsl = ${DESTBINDIR}/gff3ToPsl
pslRecalcMatch = ${DESTBINDIR}/pslRecalcMatch
pslCheck = ${DESTBINDIR}/pslCheck
pslStats = ${DESTBINDIR}/pslStats
pslPretty = ${DESTBINDIR}/pslPretty

# sh commands to deal with gff3ToPsl shouldhave failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all::

test:: patch1test grch38ToGrch37Test

patch1test: mkout
	${gff3ToPsl} input/chrom.sizes input/chrom.sizes input/haploMaps.gff stdout | sort -k 14 >  output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff expected/$@.psl output/$@.psl
#	some more extensive testing commented out
#	${pslRecalcMatch} output/$@.psl /cluster/data/hg38/hg38.2bit /cluster/data/hg38/hg38.2bit output/$@.2.psl
#	${pslCheck} -verbose=0 output/$@.2.psl
#	${pslStats} output/$@.2.psl output/$@.2.stats
#	diff expected/$@.2.stats output/$@.2.stats

grch38ToGrch37Test:
	${gff3ToPsl} input/GRCh37.p13.sizes input/GRCh38.p2.sizes input/GRCh38.p2-GRCh37.p13.gff stdout | sort -k14 -k16n -k17n > output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	diff expected/$@.psl output/$@.psl

mkout:
	@mkdir -p output

clean::
	rm -rf output
