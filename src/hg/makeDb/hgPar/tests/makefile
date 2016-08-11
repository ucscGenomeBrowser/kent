kentSrc = ../../../..
include ../../../../inc/common.mk

hgPar = ${DESTBINDIR}/hgPar

all::

test:: 
	@mkdir -p output
	${hgPar} -fileOutput hg18 input/test.parspec output/par.bed
	diff expected/par.bed output/par.bed

clean::
	rm -f par.tab

