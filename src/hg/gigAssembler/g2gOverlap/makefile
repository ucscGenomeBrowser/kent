kentSrc = ../../..
include ../../../inc/common.mk


L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = g2gOverlap.o 

g2gOverlap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/g2gOverlap $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/g2gOverlap${EXE}

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
