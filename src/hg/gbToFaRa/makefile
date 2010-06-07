include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = gbToFaRa.o

gbToFaRa: $O ../../lib/${MACHTYPE}/jkweb.a 
	${CC} ${COPT} ${CFLAGS} -O -o ${BINDIR}/gbToFaRa $O ${MYLIBS} $L

lib:
	cd ../../lib && ${MAKE}

test: 
	${MKDIR} output
	gbToFaRa input/test.fil output/test.fa output/test.ra output/test.ta input/test.gb
	diff output/test.fa expected/test.fa
	diff output/test.ra expected/test.ra
	diff output/test.ta expected/test.ta
	rm -r output

clean::
	rm -f gbToFaRa ${O}
