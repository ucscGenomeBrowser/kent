include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = vgChangeContributors.o

vgChangeContributors: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgChangeContributors $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgChangeContributors

clean:
	rm -f $O
