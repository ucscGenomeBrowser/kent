include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = catDir.o

catDir: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/catDir $O $(MYLIBS) $L

clean::
	rm -f ${O}
