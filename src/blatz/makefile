include ../inc/common.mk

L = -lm
MYLIBDIR = ../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = align.o bzp.o gapless.o index.o output.o

all: blatz blatzClient blatzServer 
	echo made

blatz: standalone.o $O ${MYLIBS}
	${CC} ${COPT} -o blatz standalone.o $O ${MYLIBS} $L
	strip blatz

blatzServer: server.o $O ${MYLIBS}
	${CC} ${COPT} -o blatzServer server.o $O ${MYLIBS} $L
	strip blatzServer

blatzClient: client.o bzp.o ${MYLIBS}
	${CC} ${COPT} -o blatzClient client.o bzp.o ${MYLIBS} $L
	strip blatzClient

clean:
	rm -f $O
