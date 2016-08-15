kentSrc = ../..
include ../../inc/common.mk

O = broadData.o paraLib.o paraMessage.o machSpec.o jobResult.o

${MACHTYPE}/paralib.a: ${O} libdir
	ar rcu ${MACHTYPE}/paralib.a ${O}
	ranlib ${MACHTYPE}/paralib.a

libdir::
	${MKDIR} ${MACHTYPE}

clean::
	rm -f ${O}
