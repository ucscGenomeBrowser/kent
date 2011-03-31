include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = bottleneck.o

bottleneck: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bottleneck $O $(MYLIBS) ${L}
	${STRIP} ${BINDIR}/bottleneck${EXE}

clean::
	rm -f ${O}
