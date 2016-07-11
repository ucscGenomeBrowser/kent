kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppWhitehead.o 

ppWhitehead: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ppWhitehead $O $(MYLIBS) $L
	mv ppWhitehead ${BINDIR}

clean::
	rm -f ${O}
