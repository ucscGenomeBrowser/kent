include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../../src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFlyBaseToUcsc.o

faFlyBaseToUcsc: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faFlyBaseToUcsc $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/faFlyBaseToUcsc${EXE}
