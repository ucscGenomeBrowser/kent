include ../../inc/common.mk

L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  ../lib/$(MACHTYPE)/paralib.a $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = broadHub.o

broadHub: $O $(MYLIBS)
	gcc -ggdb -o ../bin/broadHub $O $(MYLIBS) $L
	strip ../bin/broadHub
