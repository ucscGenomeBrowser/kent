include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faNcbiToUcsc.o

faNcbiToUcsc: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faNcbiToUcsc $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/faNcbiToUcsc
