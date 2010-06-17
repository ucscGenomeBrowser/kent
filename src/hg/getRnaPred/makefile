include ../../inc/common.mk

L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = getRnaPred.o

getRnaPred: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/getRnaPred $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/getRnaPred${EXE}

clean::
	rm -f ${O}
