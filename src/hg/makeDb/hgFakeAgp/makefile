include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgFakeAgp.o

hgFakeAgp: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hgFakeAgp $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/hgFakeAgp

clean:
	rm -f $O
