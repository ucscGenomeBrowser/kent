include ../../inc/common.mk
HG_WARN = -Wall


O = faSplit.o 

faSplit: ${O}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/faSplit $O \
	../../lib/${MACHTYPE}/jkweb.a
	${STRIP} ${HOME}/bin/${MACHTYPE}/faSplit${EXE}

debug:
	${CC} ${COPT} -o faSplit $O ../../lib/${MACHTYPE}/jkweb.a

clean::
	rm -f ${O} faSplit


