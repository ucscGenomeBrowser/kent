kentSrc = ../../../..
include ../../../../inc/common.mk

O = encodeDataWarehouse.o edwBamFileFromRa.o edwFastqFileFromRa.o edwLib.o edwQaWigSpotFromRa.o

${MACHTYPE}/edwlib.a: ${O}
	mkdir -p ${MACHTYPE}
	ar rcu ${MACHTYPE}/edwlib.a ${O}
	ranlib ${MACHTYPE}/edwlib.a

clean::
	rm -f ${O} ${MACHTYPE}/edwlib.a
