include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ffaToFa.o

ffaToFa: $O ../../lib/$(MACHTYPE)/jkweb.a 
	${CC} ${COPT} ${CFLAGS} -O -o ${BINDIR}/ffaToFa $O $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
