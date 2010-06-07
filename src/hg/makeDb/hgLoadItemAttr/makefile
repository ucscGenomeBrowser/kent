include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadItemAttr.o

hgLoadItemAttr: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgLoadItemAttr $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadItemAttr${EXE}

clean:
	rm -f $O
