kentSrc = ..
include ../inc/common.mk

L += -lm
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = olly.o

olly: $O $(MYLIBS)
#	${CC} ${COPT} ${CFLAGS} -o olly $O $(MYLIBS) $L
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/olly $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/olly${EXE}

clean::
	rm -f ${O}
