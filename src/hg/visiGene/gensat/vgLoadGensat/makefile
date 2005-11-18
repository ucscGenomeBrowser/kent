include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = vgLoadGensat.o gs.o

vgLoadGensat: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/vgLoadGensat $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/vgLoadGensat

clean:
	rm -f $O
