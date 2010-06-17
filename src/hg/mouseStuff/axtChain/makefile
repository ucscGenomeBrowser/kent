include ../../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a 

O = axtChain.o

axtChain: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtChain $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtChain${EXE}

clean::
	rm -f ${O}
