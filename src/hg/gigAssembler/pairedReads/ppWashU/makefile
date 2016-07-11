kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a $(MYLIBDIR)/jkhgap.a

O = ppWashU.o

ppWashU: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ppWashU $O $(MYLIBS) $L
	mv ppWashU ${BINDIR}

clean::
	rm -f ${O}
