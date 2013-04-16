include ../../../../inc/common.mk

gff3ToPsl = gff3ToPsl

# sh commands to deal with gff3ToPsl shouldhave failed and didn't 
cmdShouldFail = echo "Error: command should have failed" >&2; false

all::

test:: patch1test 

patch1test: mkout
	${gff3ToPsl} input/locusNameAndSize.txt input/GL339450_CM000671.gff output/$@.psl
	diff expected/$@.psl output/$@.psl
	pslRecalcMatch output/$@.psl /cluster/data/hg19/hg19.2bit input/GL339449.2bit output/$@.2.psl
	pslCheck -verbose=0 output/$@.2.psl
	pslPretty output/$@.2.psl /cluster/data/hg19/hg19.2bit input/GL339449.2bit output/$@.2.axt
	diff expected/$@.2.axt output/$@.2.axt

mkout:
	@mkdir -p output

clean::
	rm -rf output
