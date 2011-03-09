include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mafFindZero.o

mafFindZero: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o mafFindZero $O $(MYLIBS) $L
	${STRIP} mafFindZero${EXE}

clean::
	rm -f ${O}
