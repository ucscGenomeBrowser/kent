include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatFtpImages.o

gensatFtpImages: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gensatFtpImages $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gensatFtpImages

clean:
	rm -f $O
