include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spTest.o

spTest: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/spTest $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/spTest

test: 
	mkdir -p output
	spTest sp092903 Q9FFH7 > output/Q9FFH7
	diff expected/Q9FFH7 output/Q9FFH7
	spTest sp092903 P29312 > output/P29312
	diff expected/P29312 output/P29312
	rm -r output
