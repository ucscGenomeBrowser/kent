include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkableBorf.o

checkableBorf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/checkableBorf $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/checkableBorf${EXE}

clean::
	rm -f ${O}
