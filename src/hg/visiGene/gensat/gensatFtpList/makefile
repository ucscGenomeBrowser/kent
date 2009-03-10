include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatFtpList.o

gensatFtpList: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatFtpList $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gensatFtpList${EXE}

clean:
	rm -f $O
