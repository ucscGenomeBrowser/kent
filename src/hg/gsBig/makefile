include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gsBig.o

gsBig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gsBig $O $(MYLIBS) $L
#${STRIP} ${BINDIR}/gsBig${EXE}

compile:: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o gsBig $O $(MYLIBS) $L
    
clean::
	rm -f ${O} gsBig
