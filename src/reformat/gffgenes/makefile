include ../../inc/common.mk

XINC=-DGZ_FILENAMES=1

O = gffgenes.o 

${HOME}/bin/${MACHTYPE}/gffgenes: ${O}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gffgenes $O \
		../../lib/${MACHTYPE}/jkweb.a
	strip ${HOME}/bin/${MACHTYPE}/gffgenes

clean:
	rm -f ${O}
