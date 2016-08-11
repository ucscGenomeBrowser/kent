kentSrc = ../../../..
include ../../../../inc/common.mk

O = cdw.o cdwBamFileFromRa.o cdwFastqFileFromRa.o cdwLib.o cdwQaWigSpotFromRa.o cdwStep.o \
	cdwTagStorm.o cdwValid.o cdwVcfFileFromRa.o 

${MACHTYPE}/cdwlib.a: ${O}
	mkdir -p ${MACHTYPE}
	ar rcu ${MACHTYPE}/cdwlib.a ${O}
	ranlib ${MACHTYPE}/cdwlib.a

clean::
	rm -f ${O} ${MACHTYPE}/cdwlib.a

cdwWebBrowse::
	make && cd ../cdwWebBrowse && make && cd ../lib
