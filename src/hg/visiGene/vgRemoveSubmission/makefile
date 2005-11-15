include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgRemoveSubmission.o

vgRemoveSubmission: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgRemoveSubmission $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgRemoveSubmission

clean:
	rm -f $O
