include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkSgdSync.o

checkSgdSync: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkSgdSync $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/checkSgdSync${EXE}
