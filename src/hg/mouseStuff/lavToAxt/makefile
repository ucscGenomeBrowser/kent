include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = lavToAxt.o

lavToAxt: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/lavToAxt $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/lavToAxt${EXE}

clean::
	rm -f ${O}
