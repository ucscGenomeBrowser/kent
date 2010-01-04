include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}
L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgChroms.o

hgChroms: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o hgChroms $O $(MYLIBS) $L
	${MKDIR} ${BINDIR}
	cp -f hgChroms ${BINDIR}
	${STRIP} ${BINDIR}/hgChroms${EXE}

clean::
	rm -f ${O} hgChroms ${BINDIR}/hgChroms
#	cd tests && ${MAKE} clean
