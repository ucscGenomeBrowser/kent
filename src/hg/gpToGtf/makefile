include ../../inc/common.mk


L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gpToGtf.o

gpToGtf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gpToGtf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/gpToGtf${EXE}

clean::
	rm -f ${O}
