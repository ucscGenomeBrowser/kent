include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gcForBed.o

gcForBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gcForBed $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/gcForBed${EXE}

clean::
	rm -f ${O}
