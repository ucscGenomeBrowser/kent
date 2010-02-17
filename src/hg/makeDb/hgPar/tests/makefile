include ../../../../inc/common.mk

all::

test:: 
	@mkdir -p output
	hgPar -fileOutput hg18 input/test.parspec output/par.psl
	pslCheck -verbose=0 output/par.psl
	diff expected/par.psl output/par.psl

clean::
	rm -f par.tab

