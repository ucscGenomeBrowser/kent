include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spTest.o

spTest: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/spTest $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/spTest

test:
	spTest sp092903 Q9FFH7
