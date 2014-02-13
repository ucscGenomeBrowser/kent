include ../../../../inc/common.mk

gff3ToPsl = ${DESTBINDIR}/gff3ToPsl
pslRecalcMatch = ${DESTBINDIR}/pslRecalcMatch
pslCheck = ${DESTBINDIR}/pslCheck
pslStats = ${DESTBINDIR}/pslStats
pslPretty = ${DESTBINDIR}/pslPretty

# sh commands to deal with gff3ToPsl shouldhave failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all::

test:: patch1test 

patch1test: mkout
	${gff3ToPsl} input/chrom.sizes.gz input/haploMaps.gff.gz stdout | sort -k 14 >  output/$@.psl
	${pslCheck} -verbose=0 output/$@.psl
	zcat expected/$@.psl.gz | diff /dev/stdin output/$@.psl
#	some more extensive testing commented out
#	${pslRecalcMatch} output/$@.psl /cluster/data/hg38/hg38.2bit /cluster/data/hg38/hg38.2bit output/$@.2.psl
#	${pslCheck} -verbose=0 output/$@.2.psl
#	${pslStats} output/$@.2.psl output/$@.2.stats
#	diff expected/$@.2.stats output/$@.2.stats

mkout:
	@mkdir -p output

clean::
	rm -rf output
