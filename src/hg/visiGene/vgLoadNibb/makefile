include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = vgLoadNibb.o

vgLoadNibb: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgLoadNibb $O ${MYLIBS} $L
	strip ${BINDIR}/vgLoadNibb

clean:
	rm -f $O
