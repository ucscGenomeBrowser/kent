include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitFaIntoContigs.o

splitFaIntoContigs: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/splitFaIntoContigs $O ${MYLIBS} ${L}

clean:
	rm -f *.o ${BINDIR}/splitFaIntoContigs ./splitFaIntoContigs
