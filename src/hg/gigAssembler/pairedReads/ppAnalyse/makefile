kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppAnalyse.o 

ppAnalyse: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ppAnalyse $O $(MYLIBS) $L
	mv ppAnalyse ${BINDIR}

clean::
	rm -f ${O}
