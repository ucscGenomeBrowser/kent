kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslSplit.o

pslSplit: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslSplit $O $(MYLIBS) $L


lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O} pslSplit
