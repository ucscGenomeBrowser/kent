kentSrc = ../..
include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

XINC=-I../../hg/inc

O = wigAsciiToBinary.o

A = wigAsciiToBinary

all::	${A} wigZoom wigBedToBinary

$A: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/${A}${EXE}

wigZoom: wigZoom.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigZoom wigZoom.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/wigZoom${EXE}

wigBedToBinary: wigBedToBinary.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/wigBedToBinary wigBedToBinary.o ${MYLIBS} $L

test:
	@echo "The testing is out of date and needs to be fixed"

#	cd test_data && ./runtest

clean:
	rm -f ${BINDIR}/${A} $O \
	${BINDIR}/wigZoom wigZoom.o \
	${BINDIR}/wigBedToBinary wigBedToBinary.o \
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
