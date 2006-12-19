include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = wgetMd5.o

wgetMd5: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wgetMd5 $O ${MYLIBS} $L
	strip ${BINDIR}/wgetMd5

clean:
	rm -f $O
