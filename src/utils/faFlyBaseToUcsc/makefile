include ../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faFlyBaseToUcsc.o

faFlyBaseToUcsc: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faFlyBaseToUcsc $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faFlyBaseToUcsc
