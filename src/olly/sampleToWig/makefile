include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = sampleToWig.o

sampleToWig: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/sampleToWig $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/sampleToWig
