include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../src/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = subsetAxt.o

DATA_FILES = blastz.mat coding.mat medium.mat promoter.mat twilight.mat

subsetAxt: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/subsetAxt $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/subsetAxt
	${MKDIR} ${HOME}/data/subsetAxt
	cp ${DATA_FILES} ${HOME}/data/subsetAxt

clean::
	rm -f ${O}
