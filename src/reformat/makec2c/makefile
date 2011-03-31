include ../../inc/common.mk

XINC=-DGZ_FILENAMES=1

O = makec2c.o 

makec2c: ${O}
	${CC} ${COPT} $O ../../lib/${MACHTYPE}/jkweb.a ${L} -o \
		${BINDIR}/makec2c
	${STRIP} ${BINDIR}/makec2c${EXE}

clean:
	rm -f ${O}
