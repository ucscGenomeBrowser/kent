kentSrc = ../..
include ../../inc/common.mk


L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O =  altGraph.o 

TESTO = test.o

altGraph: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/altGraph $O $(MYLIBS) $L

test: $(TESTO)
	${CC} ${COPT} ${CFLAGS} -o test $(TESTO) $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
