kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterBox.o boxClump.o

clusterBox: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/clusterBox $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/clusterBox${EXE}

clean::
	rm -f ${O}
