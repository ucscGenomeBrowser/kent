include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

all::	wigAsciiToBinary

O = wigAsciiToBinary.o

A = wigAsciiToBinary

$A: $O ${MYLIBS}
	gcc -ggdb -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/${A}

test:
	cd test_data && ./runtest

clean:
	rm -f ${HOME}/bin/${MACHTYPE}/${A} $O \
	test_data/chrM.ascii test_data/chr0.wib test_data/chr0.wig \
	test_data/chrM_256.ascii test_data/chrS.wib test_data/chrS.wig
