kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = scopCollapse.o

scopCollapse: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/scopCollapse $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/scopCollapse${EXE}

test:
	scopCollapse inFeat.tab inModel.tab outFeat.tab outDesc.tab outKnownToSuper.tab
	diff expectedFeat.tab outFeat.tab
	diff expectedDesc.tab outDesc.tab
	diff expectedKnownToSuper.tab outKnownToSuper.tab

clean::
	rm -f ${O}
