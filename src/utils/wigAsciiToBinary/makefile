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
	test_data/chrM_256.ascii test_data/chrS.wib test_data/chrS.wig \
	test_data/chrM_256.wib test_data/chrM_256.wig \
	test_data/chr_1K.ascii test_data/chr_1M.ascii \
	test_data/chr1M.wib test_data/chr1M.wig \
	test_data/chr_4M.ascii test_data/chr4M.wib \
	test_data/chr4M.wig test_data/chrK.wib \
	test_data/chrK.wig test_data/chrM_1K.ascii \
	test_data/chrM.ascii test_data/chrS.wib \
	test_data/chrS.wig
