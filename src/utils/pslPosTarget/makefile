include ../../inc/common.mk

L = -lm
MYLIBDIR = ../..//lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPosTarget.o

pslPosTarget: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslPosTarget $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/pslPosTarget
