include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadEranModules.o esMotif.o

hgLoadEranModules: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadEranModules $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadEranModules${EXE}

clean::
	rm -f ${O}
