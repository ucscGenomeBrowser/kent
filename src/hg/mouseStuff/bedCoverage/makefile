include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedCoverage.o

bedCoverage: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bedCoverage $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/bedCoverage${EXE}

clean::
	rm -f ${O}
