kentSrc = ../../..
include ../../../inc/common.mk

featTypes = 5utr cds 3utr
genePredHisto = ${DESTBINDIR}/genePredHisto

test: exonLenTest exonCntTest ${featTypes:%=%ExonLenTest} ${featTypes:%=%ExonCntTest}

exon%Test: mkdirs
	${genePredHisto} -ids exon$* input/basic.gp output/$@.histo
	diff expected/$@.histo output/$@.histo

%Test: mkdirs
	${genePredHisto} $* input/basic.gp output/$@.histo
	diff expected/$@.histo output/$@.histo

mkdirs:
	@mkdir -p output

clean::
	rm -rf output
