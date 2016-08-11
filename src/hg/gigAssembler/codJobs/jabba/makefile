kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = jabba.o jobDb.o

jabba: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/jabba $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/jabba${EXE}

clean::
	rm -f ${O}
