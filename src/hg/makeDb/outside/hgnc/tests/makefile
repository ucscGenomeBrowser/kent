kentSrc = ../../../../..
include ${kentSrc}/inc/common.mk
diff = diff -u

all::

test:: hg38Test

# this is a set of 100 HGNC genes
hg38Test: mkdirs
	../hgncToBigBed --unmappedTsv=output/$@.unmapped.tsv hg38 V47 input/hgnc_complete_set.part.txt \
	    output/$@.bed output/$@.bb output/$@ output/$@.locus_type.filter.txt
	diff expected/$@.bed output/$@.bed
	diff expected/$@.trixin output/$@.trixin
	diff expected/$@.locus_type.filter.txt output/$@.locus_type.filter.txt
	diff expected/$@.unmapped.tsv output/$@.unmapped.tsv


mkdirs:
	@mkdir -p output

clean::
	rm -rf output
