include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgsqlimport

O = hgsqlimport.o

hgsqlimport: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$A $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$A${EXE}

clean::
	rm -f ${O}
