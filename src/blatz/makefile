include ../inc/common.mk

L = -lm
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = align.o bzp.o gapless.o index.o output.o

all: blatz blatzClient blatzServer 
	echo made

blatz: standalone.o $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blatz standalone.o $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/blatz

blatzServer: server.o $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blatzServer server.o $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/blatzServer

blatzClient: client.o bzp.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blatzClient client.o bzp.o ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/blatzClient

clean:
	rm -f $O
