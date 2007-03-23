include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = scopCollapse.o

scopCollapse: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/scopCollapse $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/scopCollapse${EXE}

clean:
	rm -f $O

test:
	scopCollapse inFeat.tab inModel.tab outFeat.tab outDesc.tab
	diff expectedFeat.tab outFeat.tab
	diff expectedDesc.tab outDesc.tab
