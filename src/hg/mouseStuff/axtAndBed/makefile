include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtAndBed.o

axtAndBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtAndBed $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtAndBed${EXE}

clean::
	rm -f ${O}
