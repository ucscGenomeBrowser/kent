include ../../../../inc/common.mk

O = cdw.o cdwBamFileFromRa.o cdwFastqFileFromRa.o cdwLib.o cdwQaWigSpotFromRa.o cdwValid.o

${MACHTYPE}/cdwlib.a: ${O}
	mkdir -p ${MACHTYPE}
	ar rcu ${MACHTYPE}/cdwlib.a ${O}
	ranlib ${MACHTYPE}/cdwlib.a

clean::
	rm -f ${O} ${MACHTYPE}/cdwlib.a
