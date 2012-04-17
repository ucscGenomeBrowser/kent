include ../../inc/common.mk


L += 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = patCount.o

patCount: $O ../../lib/$(MACHTYPE)/jkweb.a 
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/patCount $O $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
