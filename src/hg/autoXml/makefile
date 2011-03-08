include ../../inc/common.mk

MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = autoXml.o

autoXml: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/autoXml $O $(MYLIBS) ${L}
#	${STRIP} ${BINDIR}/autoXml${EXE}

test::
	doTest

clean:
	rm -f *.o
	rm -rf test/*/out
