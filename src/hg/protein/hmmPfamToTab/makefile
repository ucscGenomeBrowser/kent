include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = hmmPfamToTab.o

hmmPfamToTab: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/hmmPfamToTab $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hmmPfamToTab${EXE}

test:
	hmmPfamToTab in.pf out.tab
	diff expected.tab out.tab

clean:
	rm -f $O
