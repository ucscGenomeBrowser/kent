include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslSplitOnTarget.o

pslSplitOnTarget: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/pslSplitOnTarget $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/pslSplitOnTarget

clean:
	rm -f $O
