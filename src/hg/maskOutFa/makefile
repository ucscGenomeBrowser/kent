include ../../inc/common.mk


L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = maskOutFa.o

maskOutFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/maskOutFa $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/maskOutFa${EXE}

clean::
	rm -f ${O}
