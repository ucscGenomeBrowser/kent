include ../../inc/common.mk

XINC=-DGZ_FILENAMES=1

O = gffgenes.o 

${HOME}/bin/${MACHTYPE}/gffgenes: ${O}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gffgenes $O \
		../../lib/${MACHTYPE}/jkweb.a
	${STRIP} ${HOME}/bin/${MACHTYPE}/gffgenes${EXE}

clean:
	rm -f ${O}
