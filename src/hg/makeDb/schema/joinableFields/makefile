include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = joinableFields.o

joinableFields: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/joinableFields $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/joinableFields${EXE}

clean:
	rm -f $O

test:
	joinableFields ../all.joiner go goaPart
