include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterBox.o boxClump.o

clusterBox: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/clusterBox $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/clusterBox${EXE}
