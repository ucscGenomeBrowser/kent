include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spToDb.o

spToDb: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/spToDb $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/spToDb

test: test1 
%:	input/%
	mkdir -p output/$@
	spToDb input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

