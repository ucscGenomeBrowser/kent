include ../inc/common.mk

L = -lm
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = align.o bzp.o gapless.o index.o output.o

all: blatz blatzClient blatzServer 
	echo made

blatz: standalone.o $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blatz standalone.o $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/blatz${EXE}

blatzServer: server.o $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blatzServer server.o $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/blatzServer${EXE}

blatzClient: client.o bzp.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blatzClient client.o bzp.o ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/blatzClient${EXE}

clean:
	rm -f $O

test:	doTest

doTest:
	blatz test/clock/human.fa test/clock/mouse.fa test/clock/test.chain
	diff test/clock/expected.chain test/clock/test.chain
	blatz test/100k/human.fa test/100k/mouse.fa test/100k/test.chain
	diff test/100k/expected.chain test/100k/test.chain
