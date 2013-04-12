include ../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = align.o bzp.o gapless.o index.o output.o

all: blatz blatzClient blatzServer 
	echo made

blatz: standalone.o $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blatz standalone.o $O ${MYLIBS} $L
#	${STRIP} ${DESTDIR}${BINDIR}/blatz${EXE}

blatzServer: server.o $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blatzServer server.o $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/blatzServer${EXE}

blatzClient: client.o bzp.o ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/blatzClient client.o bzp.o ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/blatzClient${EXE}

clean:
	rm -f $O standalone.o server.o client.o

test:	doTest

doTest:
	blatz test/clock/human.fa test/clock/mouse.fa test/clock/test.chain
	diff test/clock/expected.chain test/clock/test.chain
	blatz test/100k/human.fa test/100k/mouse.fa test/100k/test.chain
	diff test/100k/expected.chain test/100k/test.chain
