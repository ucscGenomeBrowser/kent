include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatFtpList.o

gensatFtpList: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gensatFtpList $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gensatFtpList

clean:
	rm -f $O
