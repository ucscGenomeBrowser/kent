include ../../inc/common.mk

XINC=-DGZ_FILENAMES=1

O = makec2c.o 

makec2c: ${O}
	${CC} ${COPT} $O ../../lib/${MACHTYPE}/jkweb.a -o \
		${HOME}/bin/${MACHTYPE}/makec2c
	${STRIP} ${HOME}/bin/${MACHTYPE}/makec2c${EXE}

clean:
	rm -f ${O}
