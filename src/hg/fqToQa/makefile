include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fqToQa.o

fqToQa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fqToQa $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/fqToQa${EXE}


lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
