include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtDropSelf.o

axtDropSelf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/axtDropSelf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/axtDropSelf${EXE}

clean::
	rm -f ${O}
