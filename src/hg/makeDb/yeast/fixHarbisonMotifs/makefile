include ../../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fixHarbisonMotifs.o

fixHarbisonMotifs: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fixHarbisonMotifs $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/fixHarbisonMotifs

clean:
	rm -f $O
