include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch4.o 

newStitch4: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/newStitch4 $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/newStitch4${EXE}

clean::
	rm -f ${O}
