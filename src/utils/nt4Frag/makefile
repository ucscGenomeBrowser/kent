include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = nt4Frag.o

nt4Frag: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/nt4Frag $O $(MYLIBS) $L

clean::
	rm -f ${O}
