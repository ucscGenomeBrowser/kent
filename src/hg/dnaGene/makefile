include ../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = dnaGene

O = $A.o

my:	compile
	${CC} -o ${HOME}/bin/${MACHTYPE}/${A} $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/${A}

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A
	chmod a+rx $A

clean:
	rm -f ${O} ${A}
