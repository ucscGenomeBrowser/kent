include ../../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadWiggle.o

A = hgLoadWiggle

hgLoadWiggle: $O ${MYLIBS}
	gcc -ggdb -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/${A}

test:
	cd test_data && ./runtest

clean:
	rm -f test_data/chr1_0.wib test_data/chr1_0.wig test_data/chr1_1.wib \
	test_data/chr1_1.wig test_data/chr1_2.wib test_data/chr1_2.wig \
	test_data/chr1_begin.ascii test_data/chr1_end.ascii \
	test_data/chr1_middle.ascii test_data/chrM.ascii test_data/chrM.wib \
	test_data/chrM.wig test_data/wiggle.tab \
	${O}
