include ../../inc/common.mk


L += 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = scanRa.o 

scanRa: $O ../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/scanRa $O $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
