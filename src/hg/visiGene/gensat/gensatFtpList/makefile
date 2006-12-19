include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatFtpList.o

gensatFtpList: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatFtpList $O ${MYLIBS} $L
	strip ${BINDIR}/gensatFtpList

clean:
	rm -f $O
