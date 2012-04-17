include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L += 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gbGetEntries.o 

gbGetEntries: $O ../../lib/$(MACHTYPE)/jkweb.a
	${CC} -lm -o ${BINDIR}/gbGetEntries $O $(MYLIBS) $L

test::
	(cd tests && ${MAKE} test)

clean::
	(cd tests && ${MAKE} clean)
