include ../../inc/common.mk

XINC=-DGZ_FILENAMES=1

O = gffgenes.o 

${BINDIR}/gffgenes: ${O}
	${CC} ${COPT} -o ${BINDIR}/gffgenes $O \
		../../lib/${MACHTYPE}/jkweb.a ${L}
	${STRIP} ${BINDIR}/gffgenes${EXE}

clean:
	rm -f ${O}
