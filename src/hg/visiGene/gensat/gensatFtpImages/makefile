include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatFtpImages.o

gensatFtpImages: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatFtpImages $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gensatFtpImages${EXE}

clean:
	rm -f $O
