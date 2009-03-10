include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = vgRemoveSubmission.o

vgRemoveSubmission: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgRemoveSubmission $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/vgRemoveSubmission${EXE}

clean:
	rm -f $O
