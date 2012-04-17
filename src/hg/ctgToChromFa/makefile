include ../../inc/common.mk


L += 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = ctgToChromFa.o 

ctgToChromFa: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ctgToChromFa $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/ctgToChromFa${EXE}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
