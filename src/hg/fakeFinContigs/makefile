include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fakeFinContigs.o

fakeFinContigs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fakeFinContigs $O $(MYLIBS) $L

clean::
	rm -f ${O}
