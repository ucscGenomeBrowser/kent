include ../../inc/common.mk


L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = fragPart.o 

fragPart: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fragPart $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/fragPart${EXE}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
