include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a


O = vgChangeContributors.o

vgChangeContributors: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgChangeContributors $O ${MYLIBS} $L
	strip ${BINDIR}/vgChangeContributors

clean:
	rm -f $O
