include ../../inc/common.mk


L += ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = test.o

test: lib ${O}
	${CC} ${COPT} -o test $O ${MYLIBS} $L
	@./test
	@rm -f test

compile: lib ${O}
	${CC} ${COPT} -o test $O ${MYLIBS} $L
	@./test

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f test ${O}
