kentSrc = ../..
include ../../inc/common.mk


L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = intronSizes.o

intronSizes: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/intronSizes $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/intronSizes${EXE}

clean::
	rm -f ${O}
