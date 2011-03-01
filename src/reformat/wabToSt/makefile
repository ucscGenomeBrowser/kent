include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = wabToSt.o

wabToSt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/wabToSt $O $(MYLIBS) ${L}

clean::
	rm -f ${O}
