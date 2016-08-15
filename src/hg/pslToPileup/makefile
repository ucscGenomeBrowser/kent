kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

A = pslToPileup
O = $(A).o 

$(A): $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$(A) $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$(A)${EXE}

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
