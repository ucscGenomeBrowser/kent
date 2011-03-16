include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = findContigsWithClones.o

findContigsWithClones: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/findContigsWithClones $O $(MYLIBS) $L

clean::
	rm -f ${O}
