include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadItemAttr.o

hgLoadItemAttr: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgLoadItemAttr $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgLoadItemAttr

clean:
	rm -f $O
