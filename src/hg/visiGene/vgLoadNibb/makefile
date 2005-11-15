include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = vgLoadNibb.o

vgLoadNibb: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgLoadNibb $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgLoadNibb

clean:
	rm -f $O
