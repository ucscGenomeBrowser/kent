include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafCoverage.o

mafCoverage: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/mafCoverage $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/mafCoverage

test:	test1 test2
%:	input/%.maf
	mkdir -p output
	mafCoverage hg15 input/$@.maf > output/$@.txt
	diff output/$@.txt expected/$@.txt
	rm output/$@.txt
