kentSrc = ../..
include ../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = test.o

test: ${O}
	${CC} ${COPT} -o test $O ${MYLIBS} $L
	@./test
	@rm -f test

compile: ${O}
	${CC} ${COPT} -o test $O ${MYLIBS} $L
	@./test

clean::
	rm -f test ${O}
