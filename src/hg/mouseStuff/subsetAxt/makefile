include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = subsetAxt.o

DATA_FILES = blastz.mat coding.mat medium.mat promoter.mat twilight.mat

subsetAxt: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/subsetAxt $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/subsetAxt${EXE}

copyData:
	${MKDIR} ${HOME}/data/subsetAxt
	cp ${DATA_FILES} ${HOME}/data/subsetAxt

clean::
	rm -f ${O}
