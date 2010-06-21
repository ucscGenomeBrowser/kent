include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = faFlyBaseToUcsc.o

faFlyBaseToUcsc: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faFlyBaseToUcsc $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/faFlyBaseToUcsc${EXE}

clean::
	rm -f ${O}
