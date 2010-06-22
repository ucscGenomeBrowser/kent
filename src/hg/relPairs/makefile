include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = relPairs.o 

relPairs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/relPairs $O $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
