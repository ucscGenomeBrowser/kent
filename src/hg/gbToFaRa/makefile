include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gbToFaRa.o

gbToFaRa: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -O -o $(HOME)/bin/$(MACHTYPE)/gbToFaRa $O $(MYLIBS) $L

lib:
	cd ../../lib; make

test: 
	mkdir -p output
	gbToFaRa input/test.fil output/test.fa output/test.ra output/test.ta input/test.gb
	diff output/test.fa expected/test.fa
	diff output/test.ra expected/test.ra
	diff output/test.ta expected/test.ta
	rm -r output
