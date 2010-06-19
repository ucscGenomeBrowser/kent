include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = splitFile.o

splitFile: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/splitFile $O $(MYLIBS) $L

clean::
	rm -f ${O}
