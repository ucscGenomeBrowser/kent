include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = kgGetPep
O = $A.o

$A: $O ${MYLIBS}
	${CC} ${COPT} ${HG_WARN} ${HG_DEFS} \
		-o ${HOME}/bin/${MACHTYPE}/$A $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/$A${EXE}

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A
	chmod a+rx $A

clean::
	rm -f $O $A
