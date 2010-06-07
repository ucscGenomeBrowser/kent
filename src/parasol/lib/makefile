include ../../inc/common.mk

O = broadData.o paraLib.o paraMessage.o machSpec.o

${MACHTYPE}/paralib.a: ${O}
	ar rcu ${MACHTYPE}/paralib.a ${O}
	ranlib ${MACHTYPE}/paralib.a

clean::
	rm -f ${O}
