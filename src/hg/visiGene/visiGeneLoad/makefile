include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = visiGeneLoad.o

visiGeneLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/visiGeneLoad $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/visiGeneLoad

clean:
	rm -f $O
