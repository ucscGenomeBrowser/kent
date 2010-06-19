include ../../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadSample.o

hgLoadSample: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadSample $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadSample${EXE}

clean::
	rm -f ${O}
