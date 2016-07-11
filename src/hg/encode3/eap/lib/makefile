kentSrc = ../../../..
include ../../../../inc/common.mk

O = eapDb.o eapGraph.o eapLib.o 

${MACHTYPE}/eaplib.a: ${O}
	mkdir -p ${MACHTYPE}
	ar rcu ${MACHTYPE}/eaplib.a ${O}
	ranlib ${MACHTYPE}/eaplib.a

clean::
	rm -f ${O} ${MACHTYPE}/eaplib.a
