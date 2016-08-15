kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppStats.o pPair.o

ppStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ppStats $O $(MYLIBS) $L
	mv ppStats ${BINDIR}


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
