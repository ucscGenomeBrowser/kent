include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = qacAgpLift.o

qacAgpLift: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -O -o ${BINDIR}/qacAgpLift $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/qacAgpLift${EXE}

