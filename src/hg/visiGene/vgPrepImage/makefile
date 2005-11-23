include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = vgPrepImage.o

vgPrepImage: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgPrepImage $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgPrepImage

clean:
	rm -f $O
